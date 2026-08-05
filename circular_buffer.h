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

  // Warning: function 'at' performs boundary checking, whereas operator[] does NOT.
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
  friend iterator;
  friend const_iterator;
  friend reverse_iterator;
  friend const_reverse_iterator;

  void _increment_bufferstate();
  void _decrement_bufferstate();
  
  bool _empty() const { return _size == 0; }
  bool _full() const { return _size == N; }
  size_type _map_index(size_type index) const { return (_begin + index) % N; }
  size_type _end() const { return _map_index(_size); }

  std::array<value_type, N> _buff = {};
  size_type _begin = 0;
  size_type _size = 0;

#if CBUF_USE_FREERTOS
  mutable FreeRTOSMutexWrapper _mtx;
#else
  mutable std::mutex _mtx;
#endif // CBUF_USE_FREERTOS

  void _ensure_in_range(size_type index) const
  {
    static constexpr std::string_view error_message = "Index is out of range of buffer size";
    if (index >= _size) {
#if CBUF_ENABLE_EXCEPTIONS
      throw std::out_of_range(error_message.data());
#else
      assert(false && error_message.data());
#endif
    }
  }

  void _ensure_not_empty() const
  {
    static constexpr std::string_view error_message = "Invalid operation on empty buffer";
    if (_empty()) {
#if CBUF_ENABLE_EXCEPTIONS
      throw std::length_error(error_message.data());
#else
      assert(false && error_message.data());
#endif
    }
  }
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
  _begin = _size = 0;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::size_type CircularBuffer<T, N>::size() const
{
  std::lock_guard _lck(_mtx);
  return _size;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::front()
{
  // Avoid code duplication by applying item 3 from the 'Effective C++' book
  return const_cast<reference>(static_cast<const CircularBuffer &>(*this).front());
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::back()
{
  return const_cast<reference>(static_cast<const CircularBuffer &>(*this).back());
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::front() const
{
  std::lock_guard _lck(_mtx);
  _ensure_not_empty();
  return _buff[_begin];
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::back() const
{
  std::lock_guard _lck(_mtx);
  _ensure_not_empty();
  return _end() > 0 ? _buff[_end() - 1] : _buff.back();
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::push_back(const value_type &data)
{
  std::lock_guard _lck(_mtx);
  _buff[_end()] = data;
  _increment_bufferstate();
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::push_back(value_type &&data) noexcept
{
  std::lock_guard _lck(_mtx);
  _buff[_end()] = std::move(data);
  _increment_bufferstate();
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::_increment_bufferstate()
{
  if (!_full()) {
    ++_size;
  } else {
    _begin = (_begin + 1) % N;
  }
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::pop_front()
{
  std::lock_guard _lck(_mtx);
  _ensure_not_empty();
  _decrement_bufferstate();
}

template<typename T, std::size_t N>
inline void CircularBuffer<T, N>::_decrement_bufferstate()
{
  --_size;
  _begin = (_begin + 1) % N;
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::operator[](CircularBuffer<T, N>::size_type index)
{
  return const_cast<reference>(static_cast<const CircularBuffer &>(*this).operator[](index));
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
  return const_cast<reference>(static_cast<const CircularBuffer &>(*this).at(index));
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::at(
  CircularBuffer<T, N>::size_type index) const
{
  std::lock_guard _lck(_mtx);
  _ensure_in_range(index);
  return _buff[_map_index(index)];
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::iterator CircularBuffer<T, N>::begin()
{
  std::lock_guard _lck(_mtx);
  return iterator{ this, 0 };
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::begin() const
{
  std::lock_guard _lck(_mtx);
  return const_iterator{ this, 0 };
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::iterator CircularBuffer<T, N>::end()
{
  std::lock_guard _lck(_mtx);
  return iterator{ this, _size };
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::end() const
{
  std::lock_guard _lck(_mtx);
  return const_iterator{ this, _size };
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::cbegin() const noexcept
{
  std::lock_guard _lck(_mtx);
  return const_iterator{ this, 0 };
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::cend() const noexcept
{
  std::lock_guard _lck(_mtx);
  return const_iterator{ this, _size };
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reverse_iterator CircularBuffer<T, N>::rbegin() noexcept
{
  std::lock_guard _lck(_mtx);
  return reverse_iterator{ this, 0 };
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reverse_iterator CircularBuffer<T, N>::rbegin() const noexcept
{
  std::lock_guard _lck(_mtx);
  return const_reverse_iterator{ this, 0 };
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::reverse_iterator CircularBuffer<T, N>::rend() noexcept
{
  std::lock_guard _lck(_mtx);
  return reverse_iterator{ this, _size };
}

template<typename T, std::size_t N>
inline typename CircularBuffer<T, N>::const_reverse_iterator CircularBuffer<T, N>::rend() const noexcept
{
  std::lock_guard _lck(_mtx);
  return const_reverse_iterator{ this, _size };
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
  using size_type         = typename cbuf::size_type;

private:
  friend class CircularBuffer<T, N>;

  CBufferIterator(cbuf_pointer ptrToBuffer, size_type offset) : _ptrToBuffer{ ptrToBuffer }, _offset{ offset } {}

  bool _comparable(const CBufferIterator &other) const { return (_ptrToBuffer == other._ptrToBuffer); }
  size_type _begin() const { return _ptrToBuffer->_begin; }

  cbuf_pointer _ptrToBuffer;
  size_type _offset;

public:
  reference operator*() const
  {
    if constexpr (isReverse) {
      return _ptrToBuffer->at(_ptrToBuffer->size() - _offset - 1);
    } else {
      return _ptrToBuffer->at(_offset);
    }
  }

  pointer operator->() const { return &(operator*()); }

  reference operator[](size_t index) const
  {
    return *(this + index);
  }

  CBufferIterator &operator++()
  {
    ++_offset;
    return *this;
  }

  CBufferIterator operator++(int)
  {
    auto clone = *this;
    ++_offset;
    return clone;
  }

  CBufferIterator &operator--()
  {
    --_offset;
    return *this;
  }

  CBufferIterator operator--(int)
  {
    auto clone = *this;
    --_offset;
    return clone;
  }

  friend CBufferIterator operator+(CBufferIterator lhsiter, difference_type n)
  {
    lhsiter._offset += n;
    return lhsiter;
  }

  friend CBufferIterator operator+(difference_type n, CBufferIterator rhsiter)
  {
    rhsiter._offset += n;
    return rhsiter;
  }

  CBufferIterator &operator+=(difference_type n)
  {
    _offset += n;
    return *this;
  }

  friend CBufferIterator operator-(CBufferIterator lhsiter, difference_type n)
  {
    lhsiter._offset -= n;
    return lhsiter;
  }

  friend difference_type operator-(const CBufferIterator &lhsiter, const CBufferIterator &rhsiter)
  {
    return lhsiter._offset - rhsiter._offset;
  }

  CBufferIterator &operator-=(difference_type n)
  {
    _offset -= n;
    return *this;
  }

  bool operator==(const CBufferIterator &other) const = default;
  bool operator!=(const CBufferIterator &other) const = default;

  bool operator<(const CBufferIterator &other) const
  {
    if (!_comparable(other)) return false;
    return ((_begin() + _offset) < (other._begin() + other._offset));
  }

  bool operator>(const CBufferIterator &other) const
  {
    if (!_comparable(other)) return false;
    return ((_begin() + _offset) > (other._begin() + other._offset));
  }

  bool operator<=(const CBufferIterator &other) const
  {
    if (!_comparable(other)) return false;
    return ((_begin() + _offset) <= (other._begin() + other._offset));
  }

  bool operator>=(const CBufferIterator &other) const
  {
    if (!_comparable(other)) return false;
    return ((_begin() + _offset) >= (other._begin() + other._offset));
  }
};
