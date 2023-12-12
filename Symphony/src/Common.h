#pragma once

#include <concepts>
#include <type_traits>

#include "Logger.h"
#include "Util/Exception.h"
#include "Util/YCombinator.h"

#ifdef SYMPHONY_BUILD_DLL
   #define SYMPHONY_API __declspec(dllexport)
#else
   #define SYMPHONY_API __declspec(dllimport)
#endif

namespace Symphony
{
   using Entity = uint32_t;
   using ComponentID = uint32_t;
   using ComponentIndex = size_t;

   static const constexpr Entity NULL_ENTITY = ~0U;
   static const constexpr ComponentID NULL_COMPONENT = ~0U;

   template<typename T>
   concept Component = std::is_class_v<T> && std::is_default_constructible_v<T>;

   template <typename Alloc>
   concept Allocator = requires(Alloc a, typename Alloc::value_type * p, size_t n)
   {
      typename Alloc::template rebind_alloc<int>;

      { a.allocate(n) } -> std::same_as<typename Alloc::value_type*>;
      { a.deallocate(p, n) } -> std::same_as<void>;
   };
}