#ifndef ANANAS_UTIL_UTILITY_H
#define ANANAS_UTIL_UTILITY_H

namespace util {

namespace detail {

template<typename T>
struct remove_reference
{
	typedef T type;
};

template<typename T>
struct remove_reference<T&>
{
	typedef T type;
};

} // namespace detail

template<typename T>
typename detail::remove_reference<T>::type&& move(T&& obj)
{
	return (typename detail::remove_reference<T>::type&&)obj;
}

} // namespace util

#endif // ANANAS_UTIL_UTILITY_H
