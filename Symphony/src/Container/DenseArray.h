
#include <vector>
#include <unordered_map>
#include <cassert>

namespace Symphony
{
   template<typename Key, Component Comp, typename Allocator = std::allocator<Comp>>
   class DenseArray
   {
   public:
      using ComponentVector = std::vector<Comp, Allocator>;
      using KeyToIndexMap = std::unordered_map<Key, size_t>;
      using IndexToKeyMap = std::vector<Key>;

      void Add(Key key, const Comp& component)
      {
         assert(m_keyToIndex.find(key) == m_keyToIndex.end() && "Key already exists in DenseArray");
         size_t newIndex = m_components.size();
         m_components.push_back(component);
         m_keyToIndex[key] = newIndex;
         m_indexToKey.push_back(key);
      }

      void Remove(Key key)
      {
         auto it = m_keyToIndex.find(key);
         assert(it != m_keyToIndex.end() && "Key does not exist in DenseArray");

         size_t indexToRemove = it->second;
         if (indexToRemove != m_components.size() - 1)
         {
            std::swap(m_components[indexToRemove], m_components.back());
            Key lastKey = m_indexToKey.back();
            m_keyToIndex[lastKey] = indexToRemove;
            m_indexToKey[indexToRemove] = lastKey;
         }
         m_components.pop_back();
         m_indexToKey.pop_back();
         m_keyToIndex.erase(key);
      }

      Comp& Get(Key key)
      {
         assert(m_keyToIndex.find(key) != m_keyToIndex.end() && "Key does not exist in DenseArray");
         return m_components[m_keyToIndex[key]];
      }

      Comp& GetByIndex(size_t index)
      {
         assert(index < m_components.size() && "Index out of range");
         return m_components[index];
      }

      Key GetKeyAtIndex(size_t index)
      {
         assert(index < m_indexToKey.size() && "Index out of range");
         return m_indexToKey[index];
      }

      size_t Size() const { return m_components.size(); }

      const ComponentVector& GetAllComponents() const { return m_components; }

   private:
      ComponentVector m_components;
      KeyToIndexMap m_keyToIndex;
      IndexToKeyMap m_indexToKey;
   };
}