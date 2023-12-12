#pragma once

#include <stdexcept>
#include <string>

namespace Symphony
{
   class OutOfBoundsException : public std::runtime_error
   {
   public:
      OutOfBoundsException(size_t index, size_t maxValidIndex) :
         std::runtime_error("Index " + std::to_string(index) + " out of bounds. Max valid index is " + std::to_string(maxValidIndex) + "."),
         index(index),
         maxValidIndex(maxValidIndex)
      {}

      size_t getIndex() const { return index; }
      size_t getMaxValidIndex() const { return maxValidIndex; }

   private:
      size_t index;
      size_t maxValidIndex;
   };
}