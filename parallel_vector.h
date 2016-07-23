#pragma once

#include "array_view.h"
#include <algorithm>
#include <stdint.h>

namespace utl {

namespace detail {
	/* Simple type list class. Simplified version of STL tuple; can never be instantiated. */
	template<typename... Types>
	struct type_list
	{
		static const size_t size = sizeof...(Types);
	};

	/* Extract type with given index from the type list. */
	template<size_t Index, typename TypeList>
	struct type_list_element;

	template<size_t Index, typename First, typename... Rest>
	struct type_list_element<Index, type_list<First, Rest...>> : type_list_element<Index - 1, type_list<Rest...>> {};

	template<typename First, typename... Rest>
	struct type_list_element<0, type_list<First, Rest...>>
	{
		using type = First;
	};

	template<size_t Index>
	struct type_list_element<Index, type_list<>>
	{
		static_assert(Index < 0, "Index out of range");
	};

	template<size_t Index, typename TypeList>
	using type_list_element_t = typename type_list_element<Index, TypeList>::type;

	/* Find index of the unique type inside type list. */
	template<typename Type, typename TypeList>
	struct find_type_index;

	template<typename Type, typename First, typename... Rest>
	struct find_type_index<Type, type_list<First, Rest...>>
	{
		static const size_t value = 1 + find_type_index<Type, type_list<Rest...>>::value;
	};

	template<typename Type, typename... Rest>
	struct find_type_index<Type, type_list<Type, Rest...>>
	{
		static_assert(find_type_index<Type, type_list<Rest...>>::value == sizeof...(Rest), "Type is not unique");
		static const size_t value = 0;
	};

	template<typename Type>
	struct find_type_index<Type, type_list<>>
	{
		static const size_t value = 0; // out-of-range
	};

	/* Determine whether given type is a specialization of given template. */
	template<typename T, template<typename...> typename Template>
	struct is_specialization_of : std::false_type {};

	template<template<typename...> typename Template, typename... Args>
	struct is_specialization_of<Template<Args...>, Template> : std::true_type {};

	/* Apply metafunction to first N types of the type list. State can be used to store intermediate data.
	 * Metafunction should define initial state as "initial" typedef. */
	template<size_t N, typename State, typename Metafunc, typename... Types>
	struct apply_to_all_impl;

	template<size_t N, typename State, typename Metafunc, typename First, typename... Rest>
	struct apply_to_all_impl<N, State, Metafunc, First, Rest...>
	{
		using type = typename apply_to_all_impl<N - 1, typename Metafunc::template type<State, First>, Metafunc, Rest...>::type;
	};

	template<typename State, typename Metafunc, typename... Rest>
	struct apply_to_all_impl<0, State, Metafunc, Rest...>
	{
		using type = State;
	};

	template<typename Metafunc, typename TypeList, size_t N = TypeList::size>
	struct apply_to_all;

	template<typename Metafunc, size_t N, typename... Types>
	struct apply_to_all<Metafunc, type_list<Types...>, N> : apply_to_all_impl<N, typename Metafunc::initial, Metafunc, Types...> {};

	template<typename Metafunc, typename TypeList, size_t N = TypeList::size>
	using apply_to_all_t = typename apply_to_all<Metafunc, TypeList, N>::type;

	/* Metafunction: calculate sum of sizes of types of the type list. */
	struct sum_size
	{
		using initial = std::integral_constant<size_t, 0>;

		template<typename State, typename T>
		using type = std::integral_constant<size_t, State::value + sizeof(T)>;
	};

	/* Metafunctions: calculate min/max alignment of types from the type list. */
	struct min_align
	{
		using initial = std::integral_constant<size_t, std::numeric_limits<size_t>::max()>;

		template<typename State, typename T>
		using type = std::integral_constant<size_t, (State::value < alignof(T)) ? State::value : alignof(T)>;
	};
	struct max_align
	{
		using initial = std::integral_constant<size_t, 0>;

		template<typename State, typename T>
		using type = std::integral_constant<size_t, (State::value > alignof(T)) ? State::value : alignof(T)>;
	};

	/* Utilities to construct/destroy single object. */
	template<typename T, typename... Args>
	void construct(T *mem, Args &&... args)
	{
		new(mem) T{ std::forward<Args>(args)... };
	}
	template<typename T, typename Tuple, size_t... I>
	void construct_impl(T *mem, Tuple &&args, std::index_sequence<I...>)
	{
		new(mem) T(std::get<I>(std::forward<Tuple>(args))...);
	}
	template<typename T, typename Tuple, typename = std::enable_if_t<is_specialization_of<Tuple, std::tuple>::value>>
	void construct(T *mem, Tuple &&args)
	{
		construct_impl(mem, std::forward<Tuple>(args), std::make_index_sequence<std::tuple_size<Tuple>::value>());
	}
	template<typename T>
	void destroy(T *mem)
	{
		mem->~T();
	}

	/* Call specified functor N times, passing current iteration as an argument.
	 * Expected usage: seq_call<N>::execute([...](auto iteration) { use decltype(iteration)::value statically }). */
	template<size_t N, size_t I = 0>
	struct seq_call
	{
		template<typename Func>
		static void execute(Func &&f)
		{
			f(std::integral_constant<size_t, I>());
			seq_call<N, I + 1>::execute(std::forward<Func>(f));
		}
	};

	template<size_t N>
	struct seq_call<N, N>
	{
		template<typename Func>
		static void execute(Func &&f) {}
	};


	/* Implementation of parallel vector.
	 * All the data is stored in a single memory block: first all N elements of first type, then all N elements of second type and so on.
	 * This means that i-th slice start pointer is offset by N * sum(sizeof(Tj) for j in [0,i)) from memory block start, where N is num reserved elements (capacity).
	 * The sum is compile-time constant.
	 * Note: you should avoid power-of-two capacities, since it can cause aliasing problems when accessing elements from different slices with same indices.
	 * Note: we derive privately from traits to invoke EBCO in common cases.
	 * TODO: describe exception-safety.
	 * TODO: do we need iterator? It would be quite weird and probably not very efficient... Maybe something like multi_array_view?
	 * TODO: consider what kind of insertion (construction?) operations make sense and implement. */
	template<typename TypeList, typename Traits>
	class parallel_vector_impl : private Traits
	{
	public:
		using typename Traits::size_type;

		/* Create empty vector, optionally reserving some initial space. */
		explicit parallel_vector_impl(size_type capacity = 0)
		{
			reserve(capacity);
		}

		/* Copy constructor and assignment. */
		parallel_vector_impl(const parallel_vector_impl &rhs)
		{
			insert_copy(0, rhs, 0, rhs.mSize);
		}
		parallel_vector_impl &operator=(const parallel_vector_impl &rhs)
		{
			clear();
			insert_copy(0, rhs, 0, rhs.mSize);
			return *this;
		}

		/* Move constructor and assignment. */
		parallel_vector_impl(parallel_vector_impl &&rhs)
			: mMemory(rhs.mMemory)
			, mSize(rhs.mSize)
			, mCapacity(rhs.mCapacity)
		{
			rhs.mMemory = nullptr;
			rhs.mSize = rhs.mCapacity = 0;
		}
		parallel_vector_impl &operator=(parallel_vector_impl &&rhs)
		{
			clear();
			deallocate(mMemory);

			mMemory = rhs.mMemory;
			mSize = rhs.mSize;
			mCapacity = rhs.mCapacity;

			rhs.mMemory = nullptr;
			rhs.mSize = rhs.mCapacity = 0;

			return *this;
		}

		// TODO: init-list ctor/assign ?
		// TODO: iter range ctor/assign ?
		// TODO: args to forward to Traits ctor

		~parallel_vector_impl()
		{
			clear();
			deallocate(mMemory);
		}

		/* Access single slice of the parallel vector. It is most efficient way to iterate if you need access only to a single field. */
		template<size_t Index> auto slice()
		{
			return make_array_view(slice_start<Index>(), mSize);
		}
		template<typename Type> auto slice()
		{
			return slice<find_type_index<Type, TypeList>::value>();
		}
		template<size_t Index> auto slice() const
		{
			return make_array_view(const_slice_start<Index>(), mSize);
		}
		template<typename Type> auto slice() const
		{
			return slice<find_type_index<Type, TypeList>::value>();
		}

		/* Reallocate (if necessary) memory block so that capacity is >= requested.
		 * Never reallocates to reduce size. */
		void reserve(size_type capacity)
		{
			if (capacity <= mCapacity)
				return; // nothing to do, we're already large enough...

			// adjust capacity to avoid misalignment
			capacity = adjust_capacity(capacity);

			// allocate new memory block (the only thing that can throw)
			static const constexpr size_type kSizePerElement = apply_to_all_t<sum_size, TypeList>::value;
			void *mem = allocate(capacity * kSizePerElement);

			// move all existing elements to new block (assume nothrow move)
			for_each_slice([this, mem, capacity](auto sliceIndex) {
				static constexpr const size_t kSliceIndex = decltype(sliceIndex)::value;

				auto *sliceFrom = slice_start<kSliceIndex>(mMemory, mCapacity);
				auto *sliceTo = slice_start<kSliceIndex>(mem, capacity);
				for (size_t i = 0; i < mSize; ++i)
				{
					construct(sliceTo + i, std::move(sliceFrom[i]));
					destroy(sliceFrom + i);
				}
			});

			deallocate(mMemory);
			mMemory = mem;
			mCapacity = capacity;
		}

		/* Clear by destroying all elements. Memory is not reclaimed. */
		void clear()
		{
			for_each_slice([this](auto sliceIndex) {
				static constexpr const size_t kSliceIndex = decltype(sliceIndex)::value;
				// TODO: use proper array_view accessor..
				auto *slice = slice_start<kSliceIndex>();
				for (size_t i = 0; i < mSize; ++i)
					destroy(slice + i);
			});
		}

		/* Append new element to the end. Assumes each argument is passed to corresponding type.
		 * If some types need complex construction, use std::forward_as_tuple. */
		template<typename... Args>
		void push_back(Args &&... args)
		{
			if (mSize == mCapacity)
				auto_grow();

			for_each_slice([this, argTuple = std::forward_as_tuple(std::forward<Args>(args)...)](auto sliceIndex) mutable {
				static constexpr const size_t kSliceIndex = decltype(sliceIndex)::value;
				auto *slice = slice_start<kSliceIndex>();
				construct(slice + mSize, std::get<kSliceIndex>(std::move(argTuple)));
			});

			++mSize;
		}

		/* Insert elements by copying subrange from other container. */
		template<typename Cont>
		void insert_copy(size_type insertionPoint, Cont &&other, typename std::decay_t<Cont>::size_type begin, typename std::decay_t<Cont>::size_type end)
		{
			insert_impl(insertionPoint, std::forward<Cont>(other), begin, end, [](auto &&e) -> decltype(auto) { return std::forward<decltype(e)>(e); });
		}

		/* Insert elements by moving subrange from other container. */
		template<typename Cont>
		void insert_move(size_type insertionPoint, Cont &&other, typename std::decay_t<Cont>::size_type begin, typename std::decay_t<Cont>::size_type end)
		{
			insert_impl(insertionPoint, std::forward<Cont>(other), begin, end, [](auto &&e) -> decltype(auto) { return std::move(e); });
			other.erase(begin, end);
		}

		// TODO: insert(Args...) variant ?

		/* Erase subrange of elements. */
		void erase(size_type begin, size_type end)
		{
			size_type numRemoved = end - begin;
			for_each_slice([&](auto sliceIndex) {
				static constexpr const size_t kSliceIndex = decltype(sliceIndex)::value;
				auto *slice = slice_start<kSliceIndex>();
				auto *sliceEnd = slice + mSize;
				auto *firstUninit = sliceEnd - numRemoved;
				for (auto *p = slice + begin; p < firstUninit; ++p)
					*p = std::move(p[numRemoved]);
				for (auto *p = firstUninit; p < sliceEnd; ++p)
					destroy(p);
			});
			mSize -= numRemoved;
		}

		/* Erase last element. */
		void pop_back()
		{
			erase(mSize - 1, mSize);
		}

		/* Information about size & capacity. */
		bool empty() const { return mSize == 0; }
		size_type size() const { return mSize; }
		size_type capacity() const { return mCapacity; }

	private:
		/* Heuristic to increase capacity of the memory block. */
		void auto_grow()
		{
			// try to avoid having power-of-two capacities
			reserve(std::max(2 * mCapacity + 1, mCapacity + 20));
		}

		/* Adjust requested capacity so that we don't misalign elements. */
		size_type adjust_capacity(size_type required)
		{
			static const constexpr size_t kMinAlign = apply_to_all_t<min_align, TypeList>::value;
			static const constexpr size_t kMaxAlign = apply_to_all_t<max_align, TypeList>::value;
			static_assert(kMaxAlign % kMinAlign == 0, "");

			static const constexpr size_t kMinIncrement = kMaxAlign > 0 ? kMaxAlign / kMinAlign : 1;
			static_assert((kMinIncrement & (kMinIncrement - 1)) == 0, "Should always be power-of-two");

			// round up required capacity to increment
			return (required + kMinIncrement - 1) & ~(kMinIncrement - 1);
		}

		/* Extract pointer to the beginning of a slice with given index. */
		template<size_t Index>
		static auto slice_start(void *mem, size_type capacity)
		{
			static const constexpr size_t kSizeCoeff = apply_to_all_t<sum_size, TypeList, Index>::value;
			void *sliceStart = static_cast<char *>(mem) + capacity * kSizeCoeff;
			return static_cast<type_list_element_t<Index, TypeList> *>(sliceStart);
		}
		template<size_t Index>
		static auto const_slice_start(const void *mem, size_type capacity)
		{
			static const constexpr size_t kSizeCoeff = apply_to_all_t<sum_size, TypeList, Index>::value;
			const void *sliceStart = static_cast<const char *>(mem) + capacity * kSizeCoeff;
			return static_cast<const type_list_element_t<Index, TypeList> *>(sliceStart);
		}
		template<size_t Index>
		auto slice_start()
		{
			return slice_start<Index>(mMemory, mCapacity);
		}
		template<size_t Index>
		auto const_slice_start() const
		{
			return const_slice_start<Index>(mMemory, mCapacity);
		}

		/* Execute passed functor for each slice. */
		template<typename Func>
		static void for_each_slice(Func &&f)
		{
			seq_call<TypeList::size>::execute(std::forward<Func>(f));
		}

		/* Displace all existing elements starting from insertion point, fill gap with elements from other container. Transform function is used to select copy/move. */
		template<typename Cont, typename TransformFunctor>
		void insert_impl(size_type insertionPoint, Cont &&other, typename std::decay_t<Cont>::size_type begin, typename std::decay_t<Cont>::size_type end, TransformFunctor &&transform)
		{
			size_type numDisplaced = mSize - insertionPoint;
			size_type numInserted = end - begin;
			size_type newSize = mSize + numInserted;
			if (newSize > mCapacity)
				reserve(newSize); // TODO: currently if we reserve, we move tail elements twice, which is wasteful

								  // we do insertion in 3 steps:
								  // 1. move-construct displaced elements into uninitialized memory (either until all uninitialized memory is filled, or until all elements are displaced)
								  // 2a. if there are more elements to displace, move-assign them into vacated spots
								  // 2b. otherwise if there remaining uninitialized memory spots, fill them by copy/move-constructing inserted elements
								  // 3. fill remaining vacated spots by copy/move-assigning inserted elements
			size_type numConstructedDisplaced = std::min(numDisplaced, numInserted);
			size_type numAssignedDisplaced = numDisplaced - numConstructedDisplaced;
			size_type numAssignedInserted = numConstructedDisplaced;
			size_type numConstructedInserted = numInserted - numAssignedInserted;

			for_each_slice([&](auto sliceIndex) {
				static constexpr const size_t kSliceIndex = decltype(sliceIndex)::value;

				auto *my = slice_start<kSliceIndex>() + newSize - 1;

				for (size_type i = 0; i < numConstructedDisplaced; ++i)
				{
					construct(my, std::move(*(my - numInserted)));
					--my;
				}

				for (size_type i = 0; i < numAssignedDisplaced; ++i)
				{
					*my = std::move(*(my - numInserted));
					--my;
				}

				auto *their = other.slice<kSliceIndex>().begin() + end - 1;
				for (size_type i = 0; i < numConstructedInserted; ++i)
				{
					construct(my--, transform(*their--));
				}

				for (size_type i = 0; i < numAssignedInserted; ++i)
				{
					*my-- = transform(*their--);
				}
			});

			mSize = newSize;
		}

	private:
		void		*mMemory	= nullptr;
		size_type	mSize		= 0;
		size_type	mCapacity	= 0;
	};
}

/* Parallel vector can be customized via traits structure. */
struct default_parallel_vector_traits
{
	using size_type = uint32_t;		// in most cases this is more than enough; using it instead of size_t allows storing size+capacity in single qword on x64

	// TODO: consider factoring out allocation into separate structure (but designed better than std::allocator)
	// TODO: alignment (note that calling aligned malloc for small alignments is very wasteful...)
	void *allocate(size_t bytes)
	{
		return bytes > 0 ? ::operator new(bytes) : nullptr;
	}

	void deallocate(void *ptr)
	{
		::operator delete(ptr);
	}
};

/* Parallel vector with default traits. */
template<typename... Types>
using parallel_vector = detail::parallel_vector_impl<detail::type_list<Types...>, default_parallel_vector_traits>;

}
