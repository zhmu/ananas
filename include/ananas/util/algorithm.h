#ifndef ANANAS_ALGORITHM_H
#define ANANAS_ALGORITHM_H

namespace util
{
    template<typename InputIterator, typename UnaryPredicate>
    InputIterator find_if(InputIterator first, InputIterator last, UnaryPredicate pred)
    {
        for (/* nothing */; first != last; ++first) {
            if (pred(*first))
                return first;
        }
        return last;
    }

    template<typename Container, typename UnaryPredicate>
    typename Container::iterator find_if(Container& container, UnaryPredicate pred)
    {
        return find_if(container.begin(), container.end(), pred);
    }

    template<typename InputIt, typename UnaryFunction>
    UnaryFunction for_each(InputIt first, InputIt last, UnaryFunction f)
    {
        for (/* nothing */; first != last; ++first)
            f(*first);
        return f;
    }

    template<typename Container, typename UnaryFunction>
    UnaryFunction for_each(Container& container, UnaryFunction f)
    {
        return for_each(container.begin(), container.end(), f);
    }

} // namespace util

#endif // ANANAS_ALGORITHM_H
