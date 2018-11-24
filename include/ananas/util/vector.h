#ifndef ANANAS_UTIL_VECTOR_H
#define ANANAS_UTIL_VECTOR_H

#include "utility.h"

#ifdef __Ananas__
#include "kernel/cdefs.h"
extern "C" void* memcpy(void* dst, const void* src, size_t n) __nonnull;
#endif

namespace util {

template<typename T> struct vector;

namespace detail {

template<typename T>
struct base_vector_iterator
{
	friend struct vector<T>;

	base_vector_iterator(vector<T>& v, size_t pos)
		: i_vector(&v), i_pos(pos)
	{
	}

	constexpr T* operator->() const
	{
		return &operator*();
	}

	constexpr T& operator*() const
	{
		return (*i_vector)[i_pos];
	}

	constexpr base_vector_iterator& operator+=(int n)
	{
		i_pos += n;
		return *this;
	}

	constexpr base_vector_iterator& operator-=(int n)
	{
		i_pos -= n;
		return *this;
	}

	constexpr base_vector_iterator& operator++()
	{
		return operator+=(1);
	}

	constexpr base_vector_iterator operator+(int n) const
	{
		auto s(*this);
		s += n;
		return s;
	}

	constexpr base_vector_iterator operator-(int n) const
	{
		auto s(*this);
		s -= n;
		return s;
	}

	constexpr base_vector_iterator operator++(int)
	{
		auto s(*this);
		operator++();
		return s;
	}

	constexpr base_vector_iterator& operator--()
	{
		operator-=(1);
		return *this;
	}

	constexpr base_vector_iterator operator--(int)
	{
		auto s(*this);
		operator--();
		return s;
	}

	friend constexpr bool operator==(const base_vector_iterator& a, const base_vector_iterator& b)
	{
		return a.i_vector == b.i_vector && a.i_pos == b.i_pos;
	}

	friend constexpr bool operator!=(const base_vector_iterator& a, const base_vector_iterator& b)
	{
		return !(a == b);
	}

	friend constexpr size_t operator-(const base_vector_iterator& a, const base_vector_iterator& b)
	{
		return a.i_pos - b.i_pos;
	}

private:
	vector<T>* i_vector;
	size_t i_pos;
};

} // namespace detail

template<typename T>
struct vector
{
	using iterator = detail::base_vector_iterator<T>;
	using value_type = T;

	vector() = default;
	vector(const vector& v)
	{
		operator=(v);

	}

	vector& operator=(const vector& v)
	{
		reserve(v.size());
		for(size_t n = 0; n < v.size(); n++)
			new(&v_Items[n]) value_type{v[n]};
		v_Size = v.size();
		return *this;
	}

	vector(vector&& v)
		: v_Items(v.v_Items), v_Capacity(v.v_Capacity), v_Size(v.v_Size)
	{
		v.v_Items = nullptr;
		v.v_Capacity = 0;
		v.v_Size = 0;
	}

	~vector()
	{
		delete[] v_Items;
	}

	constexpr size_t size() const
	{
		return v_Size;
	}

	constexpr size_t capacity() const
	{
		return v_Capacity;
	}

	constexpr bool empty() const
	{
		return size() == 0;
	}

	constexpr void clear()
	{
		v_Size = 0;
	}

	constexpr iterator begin()
	{
		return iterator(*this, 0);
	}

	constexpr iterator end()
	{
		return iterator(*this, size());
	}

	constexpr value_type& front()
	{
		return *begin();
	}

	constexpr const value_type& front() const
	{
		return *begin();
	}

	constexpr value_type& back()
	{
		return *(end() - 1);
	}

	constexpr const value_type& back() const
	{
		return *(end() - 1);
	}

	constexpr value_type& operator[](size_t pos)
	{
		return v_Items[pos];
	}

	constexpr const value_type& operator[](size_t pos) const
	{
		return v_Items[pos];
	}

	void reserve(size_t n)
	{
		if (n < v_Capacity)
			return;

		auto newItems = new value_type[n];
		if (v_Items != nullptr)
			memcpy(newItems, v_Items, sizeof(value_type) * v_Size);
		v_Items = newItems;
		for (auto i = v_Capacity; i < n; i++)
			new(&v_Items[i]) value_type();
		v_Capacity = n;
	}

	constexpr iterator insert(iterator position, const value_type& val)
	{
		if (position == end()) {
			reserve(v_Capacity + 1);
			position = iterator(*this, size());
		} else {
			memmove(&v_Items[position.i_pos + 1], &v_Items[position.i_pos], size() - position.i_pos);
		}

		*position = val;
		v_Size++;
		return position;
	}

	constexpr void push_back(const value_type& val)
	{
		insert(end(), val);
	}

	constexpr void emplace_back(value_type&& val)
	{
		reserve(v_Size + 1);
		new(&v_Items[v_Size]) value_type(util::move(val));
		v_Size++;
	}

	constexpr void pop_back()
	{
		v_Size--;
	}

	constexpr void resize(size_t n)
	{
		reserve(n);
		v_Size = n;
	}

	constexpr iterator erase(iterator first, iterator last)
	{
		auto count = last - first;
		if (last == end())
		{
			v_Size -= count;
			return first;
		}

		memmove(&v_Items[first.i_pos], &v_Items[last.i_pos], sizeof(value_type) * count);
		v_Size -= count;
		return first;
	}

	constexpr iterator erase(iterator pos)
	{
		return erase(pos, pos + 1);
	}

private:
	T* v_Items = nullptr;
	size_t v_Capacity = 0;
	size_t v_Size = 0;
};

} // namespace util

#endif // ANANAS_UTIL_VECTOR_H
