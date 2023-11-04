#pragma once

#include <vector>
#include <optional>
#include <cassert>

namespace Symphony
{
   template<typename Index = size_t>
   class IndirectionTable
   {
   public:
      using Value = std::optional<Index>;
      static const ValueType Undirected;

      IndirectionTable() : m_denseSize(0) {}
      ~IndirectionTable() = default;


      Index Next()
      {
         if (!m_freeList.empty())
         {
            Index index = m_freeList.back();
            m_freeList.pop_back();
            m_indirection[index] = m_denseSize++;
            return index;
         }

         m_indirection.push_back(m_denseSize);
         return m_denseSize++;
      }

      void Erase(Index index)
      {
         assert(index < m_indirection.size() && "Index out of bounds!");
         m_freeList.push_back(index);
         m_indirection[index] = Undirected;
      }

      void Put(Index sparseIndex, Index denseIndex)
      {
         assert(sparseIndex < m_indirection.size() && "Sparse index out of bounds!");
         m_indirection[sparseIndex] = denseIndex;
      }

      void Clear()
      {
         m_denseSize = 0;
         m_freeList.clear();
         m_indirection.clear();
      }

      std::optional<Index> At(Index sparseIndex) const
      {
         assert(sparseIndex < m_indirection.size() && "Sparse index out of bounds!");
         return m_indirection[sparseIndex];
      }

      auto begin() { return m_indirection.begin(); }
      auto end() { return m_indirection.end(); }
      auto begin() const { return m_indirection.begin(); }
      auto end() const { return m_indirection.end(); }

      ValueType operator[](IndexType sparseIndex) const
      {
         assert(sparseIndex < m_indirection.size() && "Index out of bounds!");
         return m_indirection[sparseIndex];
      }

      std::size_t Size() const { return m_indirection.size(); }
      std::size_t Capacity() const { return m_indirection.capacity(); }
      std::size_t DenseSize() const { return m_denseSize; }

   private:
      std::vector<Value> m_indirection;
      std::vector<Index> m_freeList;
      Index m_denseSize;
   };

   template<typename IndexType>
   const typename IndirectionTable<IndexType>::ValueType IndirectionTable<IndexType>::Undirected = std::nullopt;
}