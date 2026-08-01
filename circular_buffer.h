#pragma once

#ifndef CBUF_USE_FREERTOS
#define CBUF_USE_FREERTOS false
#endif // CBUF_USE_FREERTOS

#ifndef CBUF_ENABLE_EXCEPTIONS
#define CBUF_ENABLE_EXCEPTIONS true
#endif // CBUF_ENABLE_EXCEPTIONS

#include <mutex>
using std::lock_guard;
#if CBUF_USE_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
#else
using std::mutex;
#endif // CBUF_USE_FREERTOS

#if CBUF_ENABLE_EXCEPTIONS
#include <stdexcept>
using std::length_error, std::out_of_range;
#else
#include <cassert>
#endif // CBUF_ENABLE_EXCEPTIONS

#include <array>
using std::array;
#include <iterator>
using std::random_access_iterator_tag;
#include <type_traits>
using std::conditional;
#include <utility>
using std::move;

// Unused includes??
// #include <algorithm>
// #include <memory>

#if CBUF_USE_FREERTOS
class FreeRTOSMutexWrapper
{
public:
  explicit FreeRTOSMutexWrapper()
  {
    _mtx = xSemaphoreCreateMutexStatic(&_mtx_buf);
    configASSERT(_mtx);
  }

  ~FreeRTOSMutexWrapper() { vSemaphoreDelete(_mtx); }

  void lock() { xSemaphoreTake(_mtx, portMAX_DELAY); }
  void unlock() { xSemaphoreGive(_mtx); }

private:
  SemaphoreHandle_t _mtx;
  StaticSemaphore_t _mtx_buf;
};
#endif // CBUF_USE_FREERTOS

// Iterator forward declaration
template<typename T, std::size_t N, bool isConst = false, bool isReverse = false>
class CBufferIterator;

template<typename T, std::size_t N>
class CircularBuffer
{
public:
  using value_type      = T;
  using pointer         =       value_type *;
  using const_pointer   = const value_type *;
  using reference       =       value_type &;
  using const_reference = const value_type &;
  using difference_type =       std::ptrdiff_t;
  using size_type       =       std::size_t;

  using iterator 		            = CBufferIterator<T, N>;
  using const_iterator 	        = CBufferIterator<T, N, true>;
  using reverse_iterator 	      = CBufferIterator<T, N, false, true>;
  using const_reverse_iterator 	= CBufferIterator<T, N, true, true>;

  CircularBuffer() = default;

  // Delete move and copy operations
  CircularBuffer(const CircularBuffer &) = delete;
  CircularBuffer &operator=(const CircularBuffer &) = delete;
  CircularBuffer(CircularBuffer &&) = delete;
  CircularBuffer &operator=(CircularBuffer &&) = delete;

  void push_back(const value_type &data);
  void push_back(value_type &&data) noexcept;
  void pop_front();
  reference front();
  reference back();
  const_reference front() const;
  const_reference back() const;
  void clear();
  bool empty() const;
  bool full() const;
  size_type capacity() const { return N; };
  size_type size() const;
  size_type buffer_size() const { return sizeof(value_type) * N; }
  const_pointer data() const { return _buff.data(); }

  const_reference operator[](size_type index) const;
  reference operator[](size_type index);
  const_reference at(size_type index) const;
  reference at(size_type index);

  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;
  const_iterator cbegin() const noexcept;
  const_iterator cend() const noexcept;
  reverse_iterator rbegin() noexcept;
  const_reverse_iterator rbegin() const noexcept;
  reverse_iterator rend() noexcept;
  const_reverse_iterator rend() const noexcept;
	
private:
  void _increment_bufferstate();
  void _decrement_bufferstate();
  bool _empty() const { return _size == 0; }
  bool _full() const { return _size == N; }

  std::array<value_type, N> _buff;
  size_type _head = 0;
  size_type _tail = 0;
  size_type _size = 0;

#if CBUF_USE_FREERTOS
  mutable FreeRTOSMutexWrapper _mtx;
#else
  mutable std::mutex _mtx;
#endif // CBUF_USE_FREERTOS

  size_type _map_index(size_type index) const
  {
    if (index >= _size) {
#if CBUF_ENABLE_EXCEPTIONS
      throw std::out_of_range("Index is out of Range of buffer size");
#else
      assert(false && "Index is out of Range of buffer size");
#endif
    }
    return (index + _tail) % N;
  }

  // friend iterator;
  // friend const_iterator;
};

template<typename T, std::size_t N> 
inline bool CircularBuffer<T, N>::full() const
{
  std::lock_guard _lck(_mtx);
  return _full();
}

template<typename T, std::size_t N>
inline bool CircularBuffer<T, N>::empty() const
{
  std::lock_guard _lck(_mtx);
  return _empty();
}


template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::clear()
{
  std::lock_guard _lck(_mtx);
  _head = _tail = _size = 0;
}

template<typename T, std::size_t N>
inline size_t CircularBuffer<T, N>::size() const
{
  std::lock_guard _lck(_mtx);
  return _size;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::front()
{
  std::lock_guard _lck(_mtx);
  if (_empty()) {
#if CBUF_ENABLE_EXCEPTIONS
    throw std::length_error("front function called on empty buffer");
#else
    assert(false && "front function called on empty buffer");
#endif
  }
  return _buff[_tail];
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::back()
{
  std::lock_guard _lck(_mtx);
  if (_empty()) {
#if CBUF_ENABLE_EXCEPTIONS
    throw std::length_error("back function called on empty buffer");
#else
    assert(false && "back function called on empty buffer");
#endif
  }
  return _head == 0 ? _buff[N - 1] : _buff[_head - 1];
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::front() const
{
  std::lock_guard _lck(_mtx);
  if (_empty()) {
#if CBUF_ENABLE_EXCEPTIONS
    throw std::length_error("front function called on empty buffer");
#else
    assert(false && "front function called on empty buffer");
#endif
  }
  return _buff[_tail];
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::back() const
{
  std::lock_guard _lck(_mtx);
  if (_empty()) {
#if CBUF_ENABLE_EXCEPTIONS
    throw std::length_error("back function called on empty buffer");
#else
    assert(false && "back function called on empty buffer");
#endif
  }
  return _head == 0 ? _buff[N - 1] : _buff[_head - 1];
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::push_back(const value_type &data)
{
  std::lock_guard _lck(_mtx);
  // if(_full())
  //	_buff[_tail].~T();
  _buff[_head] = data;
  _increment_bufferstate();
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::push_back(value_type &&data) noexcept
{
  std::lock_guard _lck(_mtx);
  _buff[_head] = std::move(data);
  _increment_bufferstate();
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::_increment_bufferstate()
{
  if (_full())
    _tail = (_tail + 1) % N;
  else
    ++_size;
  _head = (_head + 1) % N;
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::pop_front()
{
  std::lock_guard _lck(_mtx);
  if (_empty()) {
#if CBUF_ENABLE_EXCEPTIONS
    throw std::length_error("pop_front called on empty buffer");
#else
    assert(false && "pop_front called on empty buffer");
#endif
  }
  _decrement_bufferstate();
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::_decrement_bufferstate()
{
  --_size;
  _tail = (_tail + 1) % N;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::operator[](CircularBuffer<T, N>::size_type index)
{
  std::lock_guard _lck(_mtx);
  return _buff[_map_index(index)];
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::operator[](
  CircularBuffer<T, N>::size_type index) const
{
  std::lock_guard _lck(_mtx);
  return _buff[_map_index(index)];
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::at(CircularBuffer<T, N>::size_type index)
{
  std::lock_guard _lck(_mtx);
  return _buff[_map_index(index)];
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::at(
  CircularBuffer<T, N>::size_type index) const
{
  std::lock_guard _lck(_mtx);
  return _buff[_map_index(index)];
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::iterator CircularBuffer<T, N>::begin()
{
  std::lock_guard _lck(_mtx);
  iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = 0;
  return iter;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::begin() const
{
  std::lock_guard _lck(_mtx);
  const_iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = 0;
  return iter;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::iterator CircularBuffer<T, N>::end()
{
  std::lock_guard _lck(_mtx);
  iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = _size;
  return iter;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::end() const
{
  std::lock_guard _lck(_mtx);
  const_iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = _size;
  return iter;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::cbegin() const noexcept
{
  std::lock_guard _lck(_mtx);
  const_iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = 0;
  return iter;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::cend() const noexcept
{
  std::lock_guard _lck(_mtx);
  const_iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = _size;
  return iter;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reverse_iterator CircularBuffer<T, N>::rbegin() noexcept
{
  std::lock_guard _lck(_mtx);
  reverse_iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = 0;
  return iter;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reverse_iterator CircularBuffer<T, N>::rbegin() const noexcept
{
  std::lock_guard _lck(_mtx);
  const_reverse_iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = 0;
  return iter;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reverse_iterator CircularBuffer<T, N>::rend() noexcept
{
  std::lock_guard _lck(_mtx);
  reverse_iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = _size;
  return iter;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reverse_iterator CircularBuffer<T, N>::rend() const noexcept
{
  std::lock_guard _lck(_mtx);
  const_reverse_iterator iter;
  iter._ptrToBuffer = this;
  iter._offset = _tail;
  iter._index = _size;
  return iter;
}

template<typename T, std::size_t N, bool isConst, bool isReverse>
class CBufferIterator
{
public:
  using iterator_category = std::random_access_iterator_tag;
  using difference_type   = std::ptrdiff_t;
  using value_type        = T;
  using reference         = typename std::conditional<isConst, const value_type &, value_type &>::type;
  using pointer           = typename std::conditional<isConst, const value_type *, value_type *>::type;
  
  using cbuf              = CircularBuffer<value_type, N>;
  using cbuf_pointer      = typename std::conditional<isConst, const cbuf *, cbuf *>::type;

  CBufferIterator() = default;

private:
  bool _comparable(const CBufferIterator &other) const { return (_ptrToBuffer == other._ptrToBuffer); }

  cbuf_pointer _ptrToBuffer = nullptr;
  std::size_t _offset = 0;
  std::size_t _index = 0;

  friend class CircularBuffer<T, N>;

public:
  reference operator*()
  {
    if constexpr (isReverse) {
      return (*_ptrToBuffer)[(_ptrToBuffer->size() - _index - 1)];
    } else {
      return (*_ptrToBuffer)[_index];
    }
  }

  pointer operator->() { return &(operator*()); }

  reference operator[](size_t index)
  {
    auto iter = *this;
    iter._index += index;
    return *iter;
  }

  CBufferIterator &operator++()
  {
    ++_index;
    return *this;
  }

  CBufferIterator operator++(int)
  {
    auto clone = *this;
    ++_index;
    return clone;
  }

  CBufferIterator &operator--()
  {
    --_index;
    return *this;
  }

  CBufferIterator operator--(int)
  {
    auto clone = *this;
    --_index;
    return clone;
  }

  friend CBufferIterator operator+(CBufferIterator lhsiter, difference_type n)
  {
    lhsiter._index += n;
    return lhsiter;
  }

  friend CBufferIterator operator+(difference_type n, CBufferIterator rhsiter)
  {
    rhsiter._index += n;
    return rhsiter;
  }

  CBufferIterator &operator+=(difference_type n)
  {
    _index += n;
    return *this;
  }

  friend CBufferIterator operator-(CBufferIterator lhsiter, difference_type n)
  {
    lhsiter._index -= n;
    return lhsiter;
  }

  friend difference_type operator-(const CBufferIterator &lhsiter, const CBufferIterator &rhsiter)
  {

    return lhsiter._index - rhsiter._index;
  }

  CBufferIterator &operator-=(difference_type n)
  {
    _index -= n;
    return *this;
  }

  bool operator==(const CBufferIterator &other) const = default;
  bool operator!=(const CBufferIterator &other) const = default;

  bool operator<(const CBufferIterator &other) const
  {
    if (!_comparable(other)) return false;
    return ((_index + _offset) < (other._index + other._offset));
  }

  bool operator>(const CBufferIterator &other) const
  {
    if (!_comparable(other)) return false;
    return ((_index + _offset) > (other._index + other._offset));
  }

  bool operator<=(const CBufferIterator &other) const
  {
    if (!_comparable(other)) return false;
    return ((_index + _offset) <= (other._index + other._offset));
  }

  bool operator>=(const CBufferIterator &other) const
  {
    if (!_comparable(other)) return false;
    return ((_index + _offset) >= (other._index + other._offset));
  }
};
