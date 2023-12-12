#pragma once

#include <type_traits>
#include <utility>

namespace Symphony
{
   template<typename Func>
   class YCombinator
   {
      constexpr YCombinator(Func recursive) noexcept(std::is_nothrow_move_constructible_v<Func>) :
         m_lambda(std::move(recursive))
      {}

      template<typename... Args>
      constexpr decltype(auto) operator()(Args&& ...args) const noexcept(std::is_nothrow_invocable_v<Func, const YCombinator&, Args...>)
      {
         return m_lambda(*this, std::forward<Args>(args)...);
      }

      template<typename... Args>
      constexpr decltype(auto) operator()(Args&& ...args) noexcept(std::is_nothrow_invocable_v<Func, YCombinator&, Args...>)
      {
         return m_lambda(*this, std::forward<Args>(args)...);
      }

   private:
      Func m_lambda;
   };
}