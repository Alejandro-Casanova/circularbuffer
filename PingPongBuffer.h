#pragma once

#include "circular_buffer.h"
using FreeRTOSMutexWrapper;

#include <mutex>
using std::lock_guard;

template <typename T, size_t N>
class PingPongBuffer
{
public:
   using buffer_type = CircularBuffer<T, N>;

   // Pushes into active buffer
   void Push(const T& item)
   {
      std::lock_guard _lck(_mtx);
      if (_use_first)
      {
         _buffer_first.push_back(item);
      }
      else
      {
         _buffer_second.push_back(item);
      }
   }

   // GetPassiveBuffer() must be called after SwapActiveBuffer() and by the
   // same thread
   void SwapActiveBuffer()
   {
      std::lock_guard _lck(_mtx);
      _use_first = !_use_first;
   }

   buffer_type& GetPassiveBuffer()
   {
      std::lock_guard _lck(_mtx);
      return _use_first ? _buffer_second : _buffer_first;
   }

   const buffer_type& GetActiveBuffer() const
   {
      std::lock_guard _lck(_mtx);
      return _use_first ? _buffer_first : _buffer_second;
   }

private:
   buffer_type _buffer_first = {};
   buffer_type _buffer_second = {};
   bool _use_first = true;
   mutable FreeRTOSMutexWrapper _mtx;
};
