#pragma once

#include <concepts>
#include <type_traits>

#include "Logger.h"

namespace Symphony
{
   using Entity = uint32_t;
   using ComponentID = uint32_t;
   using ComponentIndex = size_t;

   static const constexpr Entity NULL_ENTITY = ~0U;
   static const constexpr ComponentID NULL_COMPONENT = ~0U;

   template<typename T>
   concept Component = std::is_class_v<T> && std::is_default_constructible_v<T>;

   static constexpr size_t SPARSE_BUCKET_SHIFT = 10;
   static constexpr size_t SPARSE_BUCKET_SIZE = 1 << SPARSE_BUCKET_SHIFT;

   template <typename Alloc>
   concept Allocator = requires(Alloc a, typename Alloc::value_type * p, size_t n)
   {
      typename Alloc::template rebind_alloc<int>;

      { a.allocate(n) } -> std::same_as<typename Alloc::value_type*>;
      { a.deallocate(p, n) } -> std::same_as<void>;
   };
}