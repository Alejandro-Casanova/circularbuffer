#pragma once

#define USING_FREERTOS
// #define CIRCULAR_BUFFER_ENABLE_EXCEPTIONS

#include <mutex>
using std::lock_guard;
#ifdef USING_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
#else
using std::mutex;
#endif // USING_FREERTOS

#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
#include <stdexcept>
using std::length_error, std::out_of_range;
#else
#include <cassert>
#endif // CIRCULAR_BUFFER_ENABLE_EXCEPTIONS

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

#ifdef USING_FREERTOS
class FreeRTOSMutexWrapper {
public:
   explicit FreeRTOSMutexWrapper() {
      _mtx = xSemaphoreCreateMutexStatic(&_mtx_buf);
      configASSERT(_mtx);
   }

   ~FreeRTOSMutexWrapper() {
      vSemaphoreDelete(_mtx);
   }

   void lock()
   {
      xSemaphoreTake(_mtx, portMAX_DELAY);
   }
   void unlock()
   {
      xSemaphoreGive(_mtx);
   }

private:
   SemaphoreHandle_t _mtx;
   StaticSemaphore_t _mtx_buf;
};
#endif // USING_FREERTOS

   
template<typename T, size_t N>
class CircularBuffer {
private:
	using pointer = T*;
	using const_pointer = const T*;
	using reference = T&;
	using const_reference = const T&;
	using difference_type = ptrdiff_t;

   template <bool isConst> struct BufferIterator;
	
public:
   using value_type = T;

	CircularBuffer();
	
	void push_back(const value_type& data);
	void push_back(value_type&& data) noexcept;
	void pop_front();
	reference front();
	reference back(); 
	const_reference front() const; 
	const_reference back() const;
	void clear();
	bool empty() const;
	bool full() const;
	size_t capacity() const;
	size_t size() const;
	size_t buffer_size() const { return sizeof(value_type) * N; };
	const_pointer data() const { return _buff.data(); }
	
	const_reference operator[](size_t index) const;
	reference operator[](size_t index);
	const_reference at(size_t index) const;
	reference at(size_t index);

	using iterator = BufferIterator<false>;
	using const_iterator = BufferIterator<true>;
	
	iterator begin();
	const_iterator begin() const;
	iterator end();
	const_iterator end() const;
	const_iterator cbegin() const noexcept;
	const_iterator cend() const noexcept;
	iterator rbegin() noexcept;
	const_iterator rbegin() const noexcept;
	iterator rend() noexcept;
	const_iterator rend() const noexcept;
	
private:
	void _increment_bufferstate();
	void _decrement_bufferstate();
   bool _empty() const;
	bool _full() const;

	std::array<value_type, N> _buff;
	size_t _head = 0;
	size_t _tail = 0;
	size_t _size = 0;

#ifdef USING_FREERTOS
	mutable FreeRTOSMutexWrapper _mtx;
#else
   mutable std::mutex _mtx;
#endif // USING_FREERTOS
			
   template<bool isConst = false>
	struct  BufferIterator{
	public:
		friend class CircularBuffer<T, N>;
		using iterator_category = std::random_access_iterator_tag;
		using difference_type = ptrdiff_t;
		using value_type = T;
		using reference = typename std::conditional<isConst, const value_type&, value_type&>::type;
		using pointer = typename std::conditional<isConst, const value_type*, value_type*>::type;
		using cbuf_pointer = typename std::conditional<isConst, const CircularBuffer<value_type, N>*, CircularBuffer<value_type, N>*>::type;
	private:
		cbuf_pointer _ptrToBuffer;
		size_t _offset;
		size_t _index;
		bool _reverse;
		
		bool _comparable(const BufferIterator<isConst>& other) const{
			return (_ptrToBuffer == other._ptrToBuffer)&&(_reverse == other._reverse);
		}
		
	public:
		BufferIterator()
			:_ptrToBuffer{nullptr}, _offset{0}, _index{0}, _reverse{false}{}
		
		BufferIterator(const BufferIterator<false>& it)
			:_ptrToBuffer{it._ptrToBuffer},
			 _offset{it._offset},
			 _index{it._index},
			 _reverse{it._reverse}{}

		reference operator*(){
			if(_reverse)
				return (*_ptrToBuffer)[(_ptrToBuffer->size() - _index - 1)];
			return (*_ptrToBuffer)[_index];
		}

		pointer  operator->() { return &(operator*()); }

		reference operator[](size_t index){
			BufferIterator iter = *this;
			iter._index += index;
			return *iter;
		}

		BufferIterator& operator++(){
			++_index;
			return *this;
		}

		BufferIterator operator++(int){
			BufferIterator iter = *this;
			++_index;
			return iter;
		}

		BufferIterator& operator--(){
			--_index;
			return *this;
		}

		BufferIterator operator--(int){
			BufferIterator iter = *this;
			--_index;
			return iter;
		}	

		friend BufferIterator operator+(BufferIterator lhsiter, difference_type n){
			lhsiter._index += n;
			return lhsiter;
		}

		friend BufferIterator operator+(difference_type n, BufferIterator rhsiter){
			rhsiter._index += n;
			return rhsiter;
		}
		

		BufferIterator& operator+=(difference_type n){
			_index += n;
			return *this;
		}

		friend BufferIterator operator-(BufferIterator lhsiter, difference_type n){
			lhsiter._index -= n;
			return lhsiter;
		}

		friend difference_type operator-(const BufferIterator& lhsiter, const BufferIterator& rhsiter){
			
			return lhsiter._index - rhsiter._index;
		}

		BufferIterator& operator-=(difference_type n){
			_index -= n;
			return *this;
		}

		bool operator==(const BufferIterator& other) const{
			if (!_comparable(other))
				return false;
			return ((_index == other._index)&&(_offset == other._offset));
		}
		
		bool operator!=(const BufferIterator& other) const{
			if (!_comparable(other))
				return true;
			return ((_index != other._index)||(_offset != other._offset));
		}

		bool operator<(const BufferIterator& other) const {
			if (!_comparable(other))
				return false;
			return ((_index + _offset)<(other._index+other._offset));
		}

		bool operator>(const BufferIterator& other) const{
			if (!_comparable(other))
				return false;
			return ((_index + _offset)>(other._index+other._offset));
		}

		bool operator<=(const BufferIterator& other) const {
			if (!_comparable(other))
				return false;
			return ((_index + _offset)<=(other._index+other._offset));
		}

		bool operator>=(const BufferIterator& other) const {
			if (!_comparable(other))
				return false;
			return ((_index + _offset)>=(other._index+other._offset));
		}
	};
};

template<typename T, size_t N>
inline 
bool CircularBuffer<T, N>::full() const{
   std::lock_guard _lck(_mtx);
	return _size == N;
}

template<typename T, size_t N>
inline 
bool CircularBuffer<T, N>::empty() const{
   std::lock_guard _lck(_mtx);
	return _size == 0;
}

template<typename T, size_t N>
inline 
size_t CircularBuffer<T, N>::capacity() const{
	return N;
}

template<typename T, size_t N>
inline 
void  CircularBuffer<T, N>::clear(){
	std::lock_guard _lck(_mtx);
	_head = _tail = _size = 0;
}

template<typename T, size_t N>
inline 
size_t CircularBuffer<T, N>::size() const{
	std::lock_guard _lck(_mtx);
	return _size;
}

template<typename T, size_t N>
inline
typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::front() {
	std::lock_guard _lck(_mtx);
	if(_empty()) {
#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
		throw std::length_error("front function called on empty buffer");
#else
      assert(false && "front function called on empty buffer");
#endif
	}
	return _buff[_tail];
}

template<typename T, size_t N>
inline
typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::back() {
	std::lock_guard _lck(_mtx);
	if(_empty()) {
#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
		throw std::length_error("back function called on empty buffer");
#else
      assert(false && "back function called on empty buffer");
#endif
	}
	return _head == 0 ? _buff[N - 1] : _buff[_head - 1];
}

template<typename T, size_t N>
inline
typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::front() const{
	std::lock_guard _lck(_mtx);
	if(_empty()){
#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
      throw std::length_error("front function called on empty buffer");
#else
      assert(false && "front function called on empty buffer");
#endif
	}
	return _buff[_tail];
}

template<typename T, size_t N>
inline
typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::back() const{
	std::lock_guard _lck(_mtx);
	if(_empty()){
#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
      throw std::length_error("back function called on empty buffer");
#else
      assert(false && "back function called on empty buffer");
#endif
	}
	return _head == 0 ? _buff[N - 1] : _buff[_head - 1];
}

template<typename T, size_t N>
inline
void CircularBuffer<T, N>::push_back(const value_type& data){
	std::lock_guard _lck(_mtx);
	//if(_full())
	//	_buff[_tail].~T();
	_buff[_head] = data;
	_increment_bufferstate();
}

template<typename T, size_t N>
inline
void CircularBuffer<T, N>::push_back(value_type&& data) noexcept{
	std::lock_guard _lck(_mtx);
	_buff[_head] = std::move(data);
	_increment_bufferstate();
}

template<typename T, size_t N>
inline 
void CircularBuffer<T, N>::_increment_bufferstate(){
	if(_full())
		_tail = (_tail + 1) % N;
	else
		++_size;
	_head = (_head + 1) % N;	
}

template <typename T, size_t N> inline CircularBuffer<T, N>::CircularBuffer() 
{

}

template <typename T, size_t N> inline void CircularBuffer<T, N>::pop_front()
{
   std::lock_guard _lck(_mtx);
	if(_empty()) {
#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
      throw std::length_error("pop_front called on empty buffer");
#else
      assert(false && "pop_front called on empty buffer");
#endif
	}
	_decrement_bufferstate();
}

template<typename T, size_t N>
inline 
void CircularBuffer<T, N>::_decrement_bufferstate(){
	--_size;
	_tail = (_tail + 1) % N;
}

template <typename T, size_t N> inline bool CircularBuffer<T, N>::_empty() const
{
   return _size == 0;
}

template <typename T, size_t N> inline bool CircularBuffer<T, N>::_full() const
{
   return _size == N;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::operator[](size_t index) {
	std::lock_guard _lck(_mtx);
	if(index >= _size) {
#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
		throw std::out_of_range("Index is out of Range of buffer size");
#else
      assert(false && "Index is out of Range of buffer size");
#endif
	}
	index += _tail;
	index %= N;
	return _buff[index];
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::operator[](size_t index) const {
	std::lock_guard _lck(_mtx);
	if(index >= _size) {
#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
		throw std::out_of_range("Index is out of Range of buffer size");
#else
      assert(false && "Index is out of Range of buffer size");
#endif
	}
	index += _tail;
	index %= N;
	return _buff[index];
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::reference CircularBuffer<T, N>::at(size_t index) {
	std::lock_guard _lck(_mtx);
	if(index >= _size) {
#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
		throw std::out_of_range("Index is out of Range of buffer size");
#else
      assert(false && "Index is out of Range of buffer size");
#endif
	}
	index += _tail;
	index %= N;
	return _buff[index];
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::const_reference CircularBuffer<T, N>::at(size_t index) const {
	std::lock_guard _lck(_mtx);
	if(index >= _size) {
#ifdef CIRCULAR_BUFFER_ENABLE_EXCEPTIONS
		throw std::out_of_range("Index is out of Range of buffer size");
#else
      assert(false && "Index is out of Range of buffer size");
#endif
	}
	index += _tail;
	index %= N;
	return _buff[index];
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::iterator CircularBuffer<T, N>::begin() {
	std::lock_guard _lck(_mtx);
	iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = 0;
	iter._reverse = false;
	return iter;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::begin() const{
	std::lock_guard _lck(_mtx);
	const_iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = 0;
	iter._reverse = false;
	return iter;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::iterator CircularBuffer<T, N>::end() {
	std::lock_guard _lck(_mtx);
	iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = _size;
	iter._reverse = false;
	return iter;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::end() const{
	std::lock_guard _lck(_mtx);
	const_iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = _size;
	iter._reverse = false;
	return iter;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::cbegin() const noexcept{
	std::lock_guard _lck(_mtx);
	const_iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = 0;
	iter._reverse = false;
	return iter;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::cend() const noexcept{
	std::lock_guard _lck(_mtx);
	const_iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = _size;
	iter._reverse = false;
	return iter;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::iterator CircularBuffer<T, N>::rbegin() noexcept{
	std::lock_guard _lck(_mtx);
	iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = 0;
	iter._reverse = true;
	return iter;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::rbegin() const noexcept{
	std::lock_guard _lck(_mtx);
	const_iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = 0;
	iter._reverse = true;
	return iter;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::iterator CircularBuffer<T, N>::rend()  noexcept{
	std::lock_guard _lck(_mtx);
	iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = _size;
	iter._reverse = true;
	return iter;
}

template<typename T, size_t N>
inline 
typename CircularBuffer<T, N>::const_iterator CircularBuffer<T, N>::rend() const noexcept{
	std::lock_guard _lck(_mtx);
	const_iterator iter;
	iter._ptrToBuffer = this;
	iter._offset = _tail;
	iter._index = _size;
	iter._reverse = true;
	return iter;
}
