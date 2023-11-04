#pragma once

#include <cstring>

#include "../Common.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <type_traits>
#include <utility>

namespace Symphony
{
   class Bucket;

   template<typename _Key = Entity, typename _Value = size_t, typename _KeyAlloc = std::allocator<_Key>, typename _BucketAlloc = std::allocator<Bucket>>
   requires Allocator<_KeyAlloc>&& Allocator<_BucketAlloc>
      class SparseSet
   {
      static_assert(std::is_arithmetic_v<_Key>, "SparseSet: Key type must be a primitive type.");
      static_assert(std::is_arithmetic_v<_Value>, "SparseSet: Value type must be a primitive type.");

      class Bucket
      {
      public:
         Bucket() :
            m_size(0)
         {
            size_t alignment = std::max(alignof(_Key), alignof(_Value));
            m_data = static_cast<char*>(std::aligned_alloc(alignment, SPARSE_BUCKET_SIZE * (sizeof(_Key) + sizeof(_Value))));
            m_keys = reinterpret_cast<_Key*>(m_data);
            m_values = reinterpret_cast<_Value*>(m_data + SPARSE_BUCKET_SIZE * sizeof(_Key));
         }

         ~Bucket()
         {
            std::aligned_free(m_data);
            m_keys = nullptr;
            m_values = nullptr;
         }

         inline void Deallocate(_BucketAlloc* allocator)
         {
            if (m_keys || m_values) [[unlikely]]
            {
                  return;
            }
               if (allocator) [[likely]]
               {
                     allocator->deallocate(this);
               }
         }

         void* operator new(size_t size, _BucketAlloc& pool) { return pool.allocate(size); }

         void operator delete(void* p, _BucketAlloc& pool) { pool.deallocate(p); }

         inline bool Contains(_Key key) const { return std::binary_search(m_keys, m_keys + m_size, key); }

         [[nodiscard]] inline std::optional<_Value> Value(_Key key) const
         {
            auto it = std::lower_bound(m_keys, m_keys + m_size, key);
            if (it != m_keys + m_size && *it == key)
            {
               return m_values[std::distance(m_keys, it)];
            }
            return std::nullopt;
         }

         inline bool Insert(_Key key, _Value value)
         {
            if (m_size >= SPARSE_BUCKET_SIZE) [[unlikely]] return false;

            auto it = std::lower_bound(m_keys, m_keys + m_size, key);
            auto index = it - m_keys;

            // Shift keys and values to make space for the new k/v pair
            std::memmove(&m_keys[index + 1], &m_keys[index], (m_size - index) * sizeof(_Key));
            std::memmove(&m_values[index + 1], &m_values[index], (m_size - index) * sizeof(_Value));

            m_keys[index] = key;
            m_values[index] = value;
            ++m_size;
            return true;
         }

         inline void Remove(_Key key)
         {
            auto it = std::lower_bound(m_keys, m_keys + m_size, key);
            if (it == m_keys + m_size || *it != key) [[unlikely]] return;

            auto index = it - m_keys;

            // Shift keys and values to cover up the gap left by the removed k/v pair
            std::memmove(&m_keys[index], &m_keys[index + 1], (m_size - index - 1) * sizeof(_Key));
            std::memmove(&m_values[index], &m_values[index + 1], (m_size - index - 1) * sizeof(_Value));

            --m_size;
         }

         void Distribute(Bucket& other)
         {
            if (other == *this) return;

            size_t mid = m_size >> 1;

            // Move the latter half of this bucket's keys & values to the beginning of other
            std::move(m_keys + mid, m_keys + m_size, other.m_keys);
            std::move(m_values + mid, m_values + m_size, other.m_values);

            other.m_size = m_size - mid;
            m_size = mid;
         }

         void Merge(Bucket& other)
         {
            if (other == *this) return;

            // Move all keys & values from other to the end of this bucket
            std::move(other.m_keys, other.m_keys + other.m_size, m_keys + m_size);
            std::move(other.m_values, other.m_values + other.m_size, m_values + m_size);
            m_size += other.m_size;
         }

         void Rebalance(Bucket& other)
         {
            if (other == *this) return;

            size_t totalSize = m_size + other.m_size;
            size_t targetSize = totalSize >> 1;
            size_t entitiesToMove = 0;

            if (m_size < targetSize)
            {
               // Move entities from other to this bucket
               entitiesToMove = targetSize - m_size;

               // Move entities from the beginning of other to the end of this bucket
               std::move(other.m_keys, other.m_keys + entitiesToMove, m_keys + m_size);
               std::move(other.m_values, other.m_values + entitiesToMove, m_values + m_size);

               // Shift the entities in other to fill the gap
               std::move(other.m_keys + entitiesToMove, other.m_keys + other.m_size, other.m_keys);
               std::move(other.m_values + entitiesToMove, other.m_values + other.m_size, other.m_values);
            }
            else
            {
               // Move entities from this bucket to other
               entitiesToMove = m_size - targetSize;

               // Shift the entities in other to make room for incoming entities
               std::move_backward(other.m_keys, other.m_keys + other.m_size, other.m_keys + other.m_size + entitiesToMove);
               std::move_backward(other.m_values, other.m_values + other.m_size, other.m_values + other.m_size + entitiesToMove);

               // Move entities from the end of this bucket to the beginning of other
               std::move(m_keys + targetSize, m_keys + m_size, other.m_keys);
               std::move(m_values + targetSize, m_values + m_size, other.m_values);
            }

            m_size = targetSize;
            other.m_size = totalSize - targetSize;
         }

         inline size_t Size() const { return m_size; }

      private:
         char* m_data;
         _Key* m_keys;
         _Value* m_values;
         size_t m_size;
      };

      using EntityAllocatorType = std::allocator_traits<_KeyAlloc>::template rebind_alloc<_Key>;
      using BucketAllocatorType = std::allocator_traits<_BucketAlloc>::template rebind_alloc<Bucket>;

   public:
      class Iterator
      {
      public:
         using iterator_category = std::bidirectional_iterator_tag;
         using value_type = std::pair<_Key, _Value>;
         using difference_type = std::ptrdiff_t;
         using pointer = value_type*;
         using reference = value_type&;

         bool operator==(const Iterator& rhs) const { return m_densePtr == rhs.m_densePtr; }
         bool operator!=(const Iterator& rhs) const { return m_densePtr != rhs.m_densePtr; }

         value_type operator*() const { return { *m_densePtr, *m_sparsePtr }; }
         pointer operator->() const
         {
            thread_local value_type tempValue;
            tempValue = **this;
            return &tempValue;
         }

         Iterator& operator++()
         {
            ++m_densePtr;
            ++m_sparsePtr;
            return *this;
         }
         Iterator operator++(int)
         {
            Iterator tmp(*this);
            ++(*this);
            return tmp;
         }

         Iterator& operator--()
         {
            --m_densePtr;
            --m_sparsePtr;
            return *this;
         }
         Iterator operator--(int)
         {
            Iterator tmp(*this);
            --(*this);
            return tmp;
         }

         value_type operator[](difference_type n) { return { *(m_densePtr + n), *(m_sparsePtr + n) }; }
         const value_type operator[](difference_type n) const { return { *(m_densePtr + n), *(m_sparsePtr + n) }; }

         Iterator operator+(difference_type n) const { return Iterator(m_densePtr + n, m_sparsePtr + n); }
         Iterator operator-(difference_type n) const { return Iterator(m_densePtr - n, m_sparsePtr - n); }

         difference_type operator-(const Iterator& rhs) const { return m_densePtr - rhs.m_densePtr; }

         Iterator& operator+=(difference_type n)
         {
            m_densePtr += n;
            m_sparsePtr += n;
            return *this;
         }
         Iterator& operator-=(difference_type n)
         {
            m_densePtr -= n;
            m_sparsePtr -= n;
            return *this;
         }

         bool operator<(const Iterator& rhs) const { return m_densePtr < rhs.m_densePtr; }
         bool operator<=(const Iterator& rhs) const { return m_densePtr <= rhs.m_densePtr; }
         bool operator>(const Iterator& rhs) const { return m_densePtr > rhs.m_densePtr; }
         bool operator>=(const Iterator& rhs) const { return m_densePtr >= rhs.m_densePtr; }

      private:
         Iterator(_Key* densePtr, _Value* sparsePtr) :
            m_densePtr(densePtr),
            m_sparsePtr(sparsePtr)
         {
         }

         friend class SparseSet;

         _Key* m_densePtr;
         _Value* m_sparsePtr;
      };

      using ConstIterator = const Iterator;

      using InvalidValue = std::numeric_limits<_Value>::value;

      explicit SparseSet(size_t initialCapacity = SPARSE_BUCKET_SIZE, float growFactor = 2) :
         m_entityAllocator(_KeyAlloc()),
         m_bucketAllocator(_BucketAlloc),
         m_size(0),
         m_capacity(initialCapacity),
         m_growFactor(growFactor)
      {
         m_dense = m_entityAllocator.allocate(m_capacity);
      }

      ~SparseSet()
      {
         for (auto& [_, bucket] : m_sparse)
         {
            bucket->~Bucket();
            bucket->Deallocate(&m_bucketAllocator);
         }
         m_sparse.clear();
         m_entityAllocator.deallocate(m_dense, m_capacity);
      }

      _Value operator[](_Key key) { return Get(key); }
      const _Value operator[](_Key key) const { return Get(key); }

      size_t Insert(_Key entity, _Value value)
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         Bucket* bucket = GetOrCreateBucket(bucketIndex);
         if (bucket->Contains(offset)) return bucket->Value(offset).value_or(-1);

         // Create new bucket and distribute selected buckets elements
         if (bucket->Size() >= SPARSE_BUCKET_SIZE)
         {
            Bucket* newBucket = GetOrCreateBucket(bucketIndex + 1);
            bucket->Distribute(*newBucket);
         }

         if (m_size == m_capacity) Resize(m_capacity * m_growFactor);

         m_dense[m_size] = entity;
         bucket->Insert(offset, value);

         return m_size++;
      }

      [[nodiscard]] _Value Get(_Key entity)
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         auto bucketIt = m_sparse.find(bucketIndex);
         if (bucketIt != m_sparse.end()) return bucketIt->second->Value(offset);
         return InvalidValue;
      }

      [[nodiscard]] const _Value Get(_Key entity) const
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         auto bucketIt = m_sparse.find(bucketIndex);
         if (bucketIt != m_sparse.end()) return bucketIt->second->Value(offset);
         return InvalidValue;
      }

      void Remove(_Key entity)
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         Bucket* bucket = GetOrCreateBucket(bucketIndex);

         auto removedIndexOpt = bucket->Value(offset);
         if (!removedIndexOpt.has_value()) return;

         _Value removedIndex = removedIndexOpt.value();

         // If target entity is not last in the dense array, swap it with the last entity to maintain dense packing
         if (removedIndex != m_size - 1)
         {
            _Key last = m_dense[m_size - 1];
            m_dense[removedIndex] = last;

            auto [lastBucketIndex, lastOffset] = GetBucketIndexAndOffset(last);
            Bucket* lastBucket = m_sparse[lastBucketIndex];
            lastBucket->Remove(lastOffset);
            lastBucket->Insert(lastOffset, removedIndex);
         }

         bucket->Remove(offset);

         // If a bucket is underfilled, check if it should be merged or rebalanced with the next bucket
         if (bucket->Size() < SPARSE_BUCKET_SIZE >> 1)
         {
            auto nextBucketIter = m_sparse.find(bucketIndex + 1);
            if (nextBucketIter != m_sparse.end())
            {
               Bucket* nextBucket = nextBucketIter->second;

               // If the combined size of the current and next bucket is within limits, merge them
               if (bucket->Size() + nextBucket->Size() <= SPARSE_BUCKET_SIZE)
               {
                  bucket->Merge(*nextBucket);
                  nextBucket->Destroy();
                  m_sparse.erase(nextBucketIter);
               } // Otherwise, rebalance
               else
               {
                  bucket->Rebalance(*nextBucket);
               }
            }
         }

         --m_size;
      }

      bool Contains(_Key entity) const
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         auto bucketIt = m_sparse.find(bucketIndex);

         return bucketIt != m_sparse.end() && bucketIt->second->Contains(offset);
      }

      void Clear()
      {
         for (auto& [_, bucket] : m_sparse)
         {
            bucket->~Bucket();
            bucket->Deallocate(m_bucketAllocator);
         }
         m_sparse.clear();
         m_size = 0;
      }

      inline size_t Size() const { return m_size; }

      inline size_t Capacity() const { return m_capacity; }

      Iterator begin() { return Iterator(m_dense, m_sparse); }
      Iterator end() { return Iterator(m_dense + m_size, m_sparse + m_size); }

      ConstIterator cbegin() const { return Iterator(m_dense, m_sparse); }
      ConstIterator cend() const { return Iterator(m_dense + m_size, m_sparse + m_size); }

   private:
      [[nodiscard]] Bucket* GetOrCreateBucket(size_t bucketIndex)
      {
         auto [it, created] = m_sparse.try_emplace(bucketIndex, nullptr);
         if (created)
         {
            it->second = new(m_bucketAllocator) Bucket();
         }
         return it->second;
      }

      [[nodiscard]] inline std::pair<size_t, size_t> GetBucketIndexAndOffset(_Key entity) const { return { entity >> SPARSE_BUCKET_SHIFT, entity & (SPARSE_BUCKET_SIZE - 1) }; }

      inline void Resize(size_t newCapacity)
      {
         if (newCapacity < m_capacity) [[unlikely]] return;

         _Key* newDense = m_entityAllocator.allocate(newCapacity);
         std::move(m_dense, m_dense + m_size, newDense);
         m_entityAllocator.deallocate(m_dense, m_capacity);
         m_dense = newDense;
         m_capacity = newCapacity;
      }

      EntityAllocatorType m_entityAllocator;
      BucketAllocatorType m_bucketAllocator;

      _Key* m_dense;
      std::map<_Value, Bucket*> m_sparse;
      size_t m_size;
      size_t m_capacity;
      float m_growFactor;
   };
}
