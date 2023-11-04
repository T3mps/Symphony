#include "Common.h"

void main()
{
   auto factorial = Symphony::YCombinator([](auto self, int n) -> int { return n <= 1 ? 1 : n * self(n - 1); });

   int result = factorial(5);
}