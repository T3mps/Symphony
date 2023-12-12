#pragma once

#include "../Common.h"
#include "SparseSet.h"
#include "DenseArray.h"

namespace Symphony
{
   template<typename Entity, Component Comp>
   class PackedArray
   {
   public:
      void Add(Entity entity, const Comp& component)
      {
         if (m_sparseSet.Contains(entity))
            return;

         auto index = m_denseArray.Size();
         m_sparseSet.Insert(entity, index);
         m_denseArray.Add(index, component);
      }

      void Remove(Entity entity) {
         size_t index = m_sparseSet[entity];
         if (!index)
            return;

         m_denseArray.Remove(index);

         if (index < m_denseArray.Size())
         {
            Entity lastEntity = m_denseArray.GetKeyAtIndex(m_denseArray.Size() - 1);
            m_sparseSet.Remove(lastEntity);
            m_sparseSet.Insert(lastEntity, index);
         }

         m_sparseSet.Remove(entity);
      }

      Comp& Get(Entity entity)
      {
         auto index = m_sparseSet[entity];
         if (!index)
         {
            static Comp dummy;
            return dummy;
         }

         return m_denseArray.GetByIndex(index);
      }

      size_t Size() const { return m_denseArray.Size(); }

   private:
      SparseSet<Entity, size_t> m_sparseSet;
      DenseArray<size_t, Comp> m_denseArray;
   };
}