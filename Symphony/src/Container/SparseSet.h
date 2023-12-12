#pragma once

#include <cstring>

#include "../Common.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace Symphony
{
   class Bucket;

   template<typename Key = Entity, typename Value = size_t, typename KeyAlloc = std::allocator<Key>, typename BucketAlloc = std::allocator<Bucket>>
   requires Allocator<KeyAlloc> && Allocator<BucketAlloc>
   class SparseSet
   {
      static_assert(std::is_arithmetic_v<Key>, "SparseSet: Key type must be a primitive type.");
      static_assert(std::is_arithmetic_v<Value>, "SparseSet: Value type must be a primitive type.");

      static constexpr size_t SPARSE_BUCKET_SHIFT = 10;
      static constexpr size_t SPARSE_BUCKET_SIZE = 1 << SPARSE_BUCKET_SHIFT;

      class Bucket
      {
      public:
         Bucket() : m_size(0)
         {
            size_t alignment = std::max(alignof(Key), alignof(Value));
            
            m_data = static_cast<char*>(_aligned_malloc(SPARSE_BUCKET_SIZE * (sizeof(Key) + sizeof(Value)), alignment));
            m_keys = reinterpret_cast<Key*>(m_data);
            m_values = reinterpret_cast<Value*>(m_data + SPARSE_BUCKET_SIZE * sizeof(Key));
         }

         ~Bucket()
         {
            _aligned_free(m_data);
            m_keys = nullptr;
            m_values = nullptr;
         }

         inline void Deallocate(BucketAlloc* allocator)
         {
            if (m_keys || m_values) [[unlikely]]
               return;

            if (allocator) [[likely]]
               allocator->deallocate(this);
         }

         void* operator new(size_t size, BucketAlloc& pool) { return pool.allocate(size); }

         void operator delete(void* p, BucketAlloc& pool) { pool.deallocate(p); }

         inline bool Contains(Key key) const { return std::binary_search(m_keys, m_keys + m_size, key); }

         [[nodiscard]] inline std::optional<Value> Value(Key key) const
         {
            auto it = std::lower_bound(m_keys, m_keys + m_size, key);
            if (it && (it != m_keys + m_size && *it == key))
               return m_values[std::distance(m_keys, it)];
            return std::nullopt;
         }

         inline bool Insert(Key key, Value value)
         {
            if (m_size >= SPARSE_BUCKET_SIZE) [[unlikely]]
               return false;

            auto it = std::lower_bound(m_keys, m_keys + m_size, key);
            if (!it)
               return false;
            auto index = std::distance(m_keys, it);

            // Shift keys and values to make space for the new k/v pair
            std::memmove(&m_keys[index + 1], &m_keys[index], (m_size - index) * sizeof(Key));
            std::memmove(&m_values[index + 1], &m_values[index], (m_size - index) * sizeof(Value));

            m_keys[index] = key;
            m_values[index] = value;
            ++m_size;
            return true;
         }

         inline void Remove(Key key)
         {
            auto it = std::lower_bound(m_keys, m_keys + m_size, key);
            if (!it || it == m_keys + m_size || *it != key)
               return;

            auto index = std::distance(m_keys, it);

            // Shift keys and values to cover up the gap left by the removed k/v pair
            std::memmove(&m_keys[index], &m_keys[index + 1], (m_size - index - 1) * sizeof(Key));
            std::memmove(&m_values[index], &m_values[index + 1], (m_size - index - 1) * sizeof(Value));

            --m_size;
         }

         void Distribute(Bucket& other)
         {
            if (other == *this)
               return;

            size_t mid = m_size >> 1;

            // Move the latter half of this bucket's keys & values to the beginning of other
            std::move(m_keys + mid, m_keys + m_size, other.m_keys);
            std::move(m_values + mid, m_values + m_size, other.m_values);

            other.m_size = m_size - mid;
            m_size = mid;
         }

         void Merge(Bucket& other)
         {
            if (other == *this)
               return;

            // Move all keys & values from other to the end of this bucket
            std::move(other.m_keys, other.m_keys + other.m_size, m_keys + m_size);
            std::move(other.m_values, other.m_values + other.m_size, m_values + m_size);
            m_size += other.m_size;
         }

         void Rebalance(Bucket& other)
         {
            if (other == *this)
               return;

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
         Key* m_keys;
         Value* m_values;
         size_t m_size;
      };

      using EntityAllocatorType = std::allocator_traits<KeyAlloc>::template rebind_alloc<Key>;
      using BucketAllocatorType = std::allocator_traits<BucketAlloc>::template rebind_alloc<Bucket>;

   public:
      class Iterator
      {
      public:
         using iterator_category = std::bidirectional_iterator_tag;
         using value_type = std::pair<Key, Value>;
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
         Iterator(Key* densePtr, Value* sparsePtr) :
            m_densePtr(densePtr),
            m_sparsePtr(sparsePtr)
         {}

         friend class SparseSet;

         Key* m_densePtr;
         Value* m_sparsePtr;
      };

      using ConstIterator = const Iterator;

      using InvalidValue = std::numeric_limits<Value>::value;

      explicit SparseSet(size_t initialCapacity = SPARSE_BUCKET_SIZE, float growFactor = 2) :
         m_entityAllocator(KeyAlloc()),
         m_bucketAllocator(BucketAlloc),
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
            if (bucket)
            {
               bucket->~Bucket();
               bucket->Deallocate(&m_bucketAllocator);
            }
         }
         m_sparse.clear();
         m_entityAllocator.deallocate(m_dense, m_capacity);
      }

      Value operator[](Key key) { return Get(key); }
      const Value operator[](Key key) const { return Get(key); }

      size_t Insert(Key entity, Value value)
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         Bucket* bucket = GetOrCreateBucket(bucketIndex);
         if (!bucket)
            return m_size;
         if (bucket->Contains(offset))
            return bucket->Value(offset).value_or(-1);

         // Create new bucket and distribute selected buckets elements
         if (bucket->Size() >= SPARSE_BUCKET_SIZE)
         {
            Bucket* newBucket = GetOrCreateBucket(bucketIndex + 1);
            if (newBucket)
               bucket->Distribute(*newBucket);
         }

         if (m_size == m_capacity)
            Resize(m_capacity * m_growFactor);

         m_dense[m_size] = entity;
         bucket->Insert(offset, value);

         return m_size++;
      }

      [[nodiscard]] Value Get(Key entity)
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         auto bucketIt = m_sparse.find(bucketIndex);
         if (bucketIt && bucketIt != m_sparse.end())
            return bucketIt->second->Value(offset);
         return InvalidValue;
      }

      [[nodiscard]] const Value Get(Key entity) const
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         auto bucketIt = m_sparse.find(bucketIndex);
         if (bucketIt && bucketIt != m_sparse.end())
            return bucketIt->second->Value(offset);
         return InvalidValue;
      }

      void Remove(Key entity)
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         Bucket* bucket = GetOrCreateBucket(bucketIndex);
         if (!bucket)
            return;

         auto removedIndexOpt = bucket->Value(offset);
         if (removedIndexOpt ==  std::nullopt)
            return;

         Value removedIndex = removedIndexOpt.value();

         // If target entity is not last in the dense array, swap it with the last entity to maintain dense packing
         if (removedIndex != m_size - 1)
         {
            Key last = m_dense[m_size - 1];
            m_dense[removedIndex] = last;

            auto [lastBucketIndex, lastOffset] = GetBucketIndexAndOffset(last);
            Bucket* lastBucket = m_sparse[lastBucketIndex];
            if (lastBucket)
            {
               lastBucket->Remove(lastOffset);
               lastBucket->Insert(lastOffset, removedIndex);
            }
         }

         bucket->Remove(offset);

         // If a bucket is underfilled, check if it should be merged or rebalanced with the next bucket
         if (bucket->Size() < SPARSE_BUCKET_SIZE >> 1)
         {
            auto nextBucketIter = m_sparse.find(bucketIndex + 1);
            if (nextBucketIter && nextBucketIter != m_sparse.end())
            {
               Bucket* nextBucket = nextBucketIter->second;
               if (nextBucket)
               {
                  // If the combined size of the current and next bucket is within limits, merge them
                  if (bucket->Size() + nextBucket->Size() <= SPARSE_BUCKET_SIZE)
                  {
                     bucket->Merge(*nextBucket);
                     nextBucket->~Bucket();
                     nextBucket->Deallocate(m_bucketAllocator);
                     m_sparse.erase(nextBucketIter);
                  } // Otherwise, rebalance
                  else
                  {
                     bucket->Rebalance(*nextBucket);
                  }
               }
            }
         }
         --m_size;
      }

      bool Contains(Key entity) const
      {
         auto [bucketIndex, offset] = GetBucketIndexAndOffset(entity);
         auto bucketIt = m_sparse.find(bucketIndex);
         
         return bucketIt && bucketIt != m_sparse.end() && bucketIt->second->Contains(offset);
      }

      void Clear()
      {
         for (auto& [_, bucket] : m_sparse)
         {
            if (bucket)
            {
               bucket->~Bucket();
               bucket->Deallocate(m_bucketAllocator);
            }
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
            it->second = new(m_bucketAllocator) Bucket();
         return it->second;
      }

      [[nodiscard]] inline std::pair<size_t, size_t> GetBucketIndexAndOffset(Key entity) const { return { entity >> SPARSE_BUCKET_SHIFT, entity & (SPARSE_BUCKET_SIZE - 1) }; }

      inline void Resize(size_t newCapacity)
      {
         if (newCapacity < m_capacity) [[unlikely]]
            return;

         Key* newDense = m_entityAllocator.allocate(newCapacity);
         if (!newDense)
            return;

         std::move(m_dense, m_dense + m_size, newDense);
         m_entityAllocator.deallocate(m_dense, m_capacity);
         m_dense = newDense;
         m_capacity = newCapacity;
      }

      EntityAllocatorType m_entityAllocator;
      BucketAllocatorType m_bucketAllocator;

      Key* m_dense;
      std::map<Value, Bucket*> m_sparse;
      size_t m_size;
      size_t m_capacity;
      float m_growFactor;
   };
}
