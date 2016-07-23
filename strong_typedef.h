#pragma once

/* "Strong typedef" is similar to standard typedef, except that it synthesizes proper new type, distinct from any other.
 * The main usecase is creating "named tuples" and being able to access distinct tuple elements of "same type" by unique type name.
 *
 * Example: imagine tuple<string, string> containing first & last names. Such definition has two problems: (1) it's only possible to access
 * tuple elements by index rather than by type (since string type isn't unique), (2) there is no indication what each string means.
 * Plain typedefs (using first_name = string; using last_name = string; tuple<first_name, last_name> t) only solve the second problem (partially - such
 * definition is still equivalent to tuple<string, string>); first problem is not solved at all (still not possible to say get<first_name>(t)).
 * The solution is a strong typedef. It turns first_name and last_name into proper distinct types that are still implicitly convertible to strings.
 *
 * Note that it is possible to use strong typedefs for different things - classes, scalars, pointers, arrays, etc. */

namespace utl {

/* TODO: should we use the generic implementation for classes too? T == MyClass[N] would already use that... */
template<typename T, bool UseDerivation = std::is_class<T>::value>
struct strong_typedef_impl : public T
{
	using T::T;
};

template<typename T>
struct strong_typedef_impl<T, false>
{
	T val;

	strong_typedef_impl() {}

	template<typename... Args>
	strong_typedef_impl(Args &&... args) : val{ std::forward<Args>(args)... } {}

	operator T&() { return val; }
};

}

/* We use "..." to simplify things like STRONG_TYPEDEF(mytype, pair<int, float>). This requires NAME to be mentioned first, similar to new "using" typedefs. */
#define STRONG_TYPEDEF(NAME, ...)								\
	struct NAME : public utl::strong_typedef_impl<__VA_ARGS__>	\
	{															\
		using strong_typedef_impl::strong_typedef_impl;			\
	}
