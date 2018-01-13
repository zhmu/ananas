#ifndef ANANAS_LIST_H
#define ANANAS_LIST_H

/*
 * This implements a standard doubly-linked list structure; obtaining/removing
 * the head takes O(1), adding a new item takes O(1) and removing any item
 * takes O(1).  Iterating through the entire list takes O(n) (this also holds
 * for locating a single item).
 *
 * Each list has a 'head' and 'tail' element, and every item has a previous
 * and next pointer.
 */
template<typename T>
struct List
{
	// Pointers associated with the entry
	struct Ptr {
		Ptr() = default;
		Ptr(const Ptr&) = delete;
		Ptr(const Ptr&&) = delete;
		Ptr& operator=(const Ptr&) = delete;
		Ptr& operator=(Ptr&&) = delete;

		T* p_Prev = nullptr;
		T* p_Next = nullptr;
	};

	struct iterator {
		iterator(Ptr* s) : i_Ptr(s) { }
		Ptr* i_Ptr;

		iterator& operator++() {
			i_Ptr = i_Ptr->p_Next;
			return *this;
		}

		iterator& operator++(int) {
			Ptr s(*this);
			i_Ptr = i_Ptr->p_Next;
			return s;
		}

		iterator& operator--() {
			i_Ptr = i_Ptr->p_Prev;
			return *this;
		}

		iterator& operator--(int) {
			Ptr s(*this);
			i_Ptr = i_Ptr->p_Prev;
			return s;
		}

		T& operator*() const {
			return *static_cast<T*>(i_Ptr);
		}

		bool operator==(const iterator& rhs) const {
			return i_Ptr == rhs.i_Ptr;
		}

		bool operator!=(const iterator& rhs) const {
			return !(*this == rhs);
		}
	};

	void Append(T& item)
	{
		item.p_Next = nullptr;
		if (l_Head == nullptr) {
			item.p_Prev = nullptr;
			l_Head = &item;
		} else {
			item.p_Prev = l_Tail;
			l_Tail->p_Next = &item;
		}
		l_Tail = &item;
	}

	void Prepend(T& item)
	{
		item.p_Prev = nullptr;
		if (l_Head == NULL) {
			item.p_Next = nullptr;
			l_Tail = &item;
		} else {
			item.p_Next = l_Head;
			l_Head->p_Prev = &item;
		}
		l_Head = &item;
	}

	void PopHead()
	{
		l_Head = l_Head->p_Next;
		if (l_Head != nullptr)
			l_Head->p_Prev = nullptr;
	}

	void PopTail()
	{
		l_Tail = l_Tail->p_Prev;
		if (l_Tail != nullptr)
			l_Tail->p_Next = nullptr;
	}

	void InsertBefore(T& pos, T& item)
	{
		if (pos.p_Prev != nullptr)
			pos.p_Prev->p_Next = &item;
		item.p_Next = &pos;
		item.p_Prev = pos.p_Prev;
		pos.p_Prev = &item;
		if (l_Head == &pos)
			l_Head = &item;
	}

	void Remove(T& item)
	{
		if (item.p_Prev != nullptr)
			item.p_Prev->p_Next = item.p_Next;
		if (item.p_Next != nullptr)
			item.p_Next->p_Prev = item.p_Prev;
		if (l_Head == &item)
			l_Head = item.p_Next;
		if (l_Tail == &item)
			l_Tail = item.p_Prev;
	}

	void Clear()
	{
		l_Head = nullptr; l_Tail = nullptr;
	}

	bool IsEmpty() const
	{
		return l_Head == nullptr;
	}

	T& Head() const
	{
		return *l_Head;
	}

	T& Tail() const
	{
		return *l_Tail;
	}

	List()
	 : l_Head(nullptr), l_Tail(nullptr)
	{
	}

	List(const List&) = delete;
	List(const List&&) = delete;
	List& operator=(const List&) = delete;
	List& operator=(List&&) = delete;

	iterator begin()
	{
		return iterator(l_Head);
	}

	iterator end()
	{
		return iterator(nullptr);
	}

	T* l_Head;
	T* l_Tail;
};

#endif // ANANAS_LIST_H
