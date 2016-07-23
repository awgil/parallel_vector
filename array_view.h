#pragma once

#include <iterator>

namespace utl {

/* Array view is a non-owning contiguous range of elements. It can describe a subrange in STL array or vector, built-in array, and other similar structures.
 * It allows familiar iteration methods (range-based for, begin()/end(), data()/size(), etc.). */
template<typename T>
class array_view
{
public:
	// Exposed typedefs
	typedef T value_type;
	typedef T &reference;
	typedef const T &const_reference;
	typedef T *pointer;
	typedef const T *const_pointer;
	typedef pointer iterator;
	typedef const_pointer const_iterator;
	typedef ptrdiff_t difference_type;
	typedef size_t size_type;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	/* Default constructor creates empty range. */
	array_view() : mBegin(nullptr), mEnd(nullptr) {}

	/* Constructor: range defined by two pointers: [begin, end). */
	array_view(T *begin, T *end) : mBegin(begin), mEnd(end) {}

	/* Note: default copy/move/assignment/dtor are perfectly fine. */

	// Element access
	reference at(size_type pos) { throw_out_of_range_if(mBegin + pos >= mEnd); return mBegin[pos]; }
	const_reference at(size_type pos) const { throw_out_of_range_if(mBegin + pos >= mEnd); return mBegin[pos]; }

	reference operator[](size_type pos) { return mBegin[pos]; }
	const_reference operator[](size_type pos) const { return mBegin[pos]; }

	reference front() { return *mBegin; }
	const_reference front() const { return *mBegin; }

	reference back() { return *(mEnd - 1); }
	const_reference back() const { return *(mEnd - 1); }

	pointer data() noexcept { return mBegin; }
	const_pointer data() const noexcept { return mBegin; }

	// Iterators
	iterator begin() noexcept { return mBegin; }
	const_iterator begin() const noexcept { return mBegin; }
	const_iterator cbegin() const noexcept { return mBegin; }

	iterator end() noexcept { return mEnd; }
	const_iterator end() const noexcept { return mEnd; }
	const_iterator cend() const noexcept { return mEnd; }

	reverse_iterator rbegin() noexcept { return reverse_iterator(mEnd); }
	const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(mEnd); }
	const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(mEnd); }

	reverse_iterator rend() noexcept { return reverse_iterator(mBegin); }
	const_reverse_iterator rend() const noexcept { return const_reverse_iterator(mBegin); }
	const_reverse_iterator crend() const noexcept { return const_reverse_iterator(mBegin); }

	// Size
	bool empty() const noexcept { return mBegin == mEnd; }
	size_type size() const noexcept { return static_cast<size_type>(mEnd - mBegin); }

private:
	/* Throw std::out_of_range if condition is true. */
	void throw_out_of_range_if(bool condition)
	{
		if (condition)
			throw std::out_of_range("Index out of range");
	}

private:
	T *mBegin;
	T *mEnd;
};

/* Utilities to create array views. */
template<typename T> array_view<T> make_array_view(T *begin, T *end)
{
	return array_view<T>(begin, end);
}

template<typename T> array_view<T> make_array_view(T *start, size_t size)
{
	return array_view<T>(start, start + size);
}

template<typename T, size_t N> array_view<T> make_array_view(T (&arr)[N])
{
	return array_view<T>(arr, arr + N);
}

}
