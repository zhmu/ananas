#ifndef ANANAS_UTIL_LIST_H
#define ANANAS_UTIL_LIST_H

namespace util {

namespace detail {

// Pointers associated with each list entry
template<typename T>
struct list_node {
	list_node() = default;
	list_node(const list_node&) = delete;
	list_node(const list_node&&) = delete;
	list_node& operator=(const list_node&) = delete;
	list_node& operator=(list_node&&) = delete;

	T* p_Prev = nullptr;
	T* p_Next = nullptr;
};


template<typename T>
struct list_iterator
{
	typedef list_node<T> Ptr;

	list_iterator(Ptr* s) : i_NodePtr(s) { }
	Ptr* i_NodePtr;

	list_iterator& operator++() {
		i_NodePtr = i_NodePtr->p_Next;
		return *this;
	}

	list_iterator& operator++(int) {
		Ptr s(*this);
		i_NodePtr = i_NodePtr->p_Next;
		return s;
	}

	list_iterator& operator--() {
		i_NodePtr = i_NodePtr->p_Prev;
		return *this;
	}

	list_iterator& operator--(int) {
		Ptr s(*this);
		i_NodePtr = i_NodePtr->p_Prev;
		return s;
	}

	T& operator*() const {
		return *static_cast<T*>(i_NodePtr);
	}

	bool operator==(const list_iterator& rhs) const {
		return i_NodePtr == rhs.i_NodePtr;
	}

	bool operator!=(const list_iterator& rhs) const {
		return !(*this == rhs);
	}
};

}

/*
 * This implements a standard doubly-linked list structure; obtaining/removing
 * the head takes O(1), adding a new item takes O(1) and removing any item
 * takes O(1).  Iterating through the entire list takes O(n) (this also holds
 * for locating a single item).
 *
 * Each list has a 'head' and 'tail' element, and every item has a previous
 * and next pointer (contained in NodePtr, so you need to derive from that)
 */
template<typename T>
struct List
{
	typedef typename detail::list_node<T> NodePtr;
	typedef typename detail::list_iterator<T> iterator;
	typedef typename detail::list_iterator<const T> const_iterator;

	void push_back(T& item)
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

	void push_front(T& item)
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

	void pop_front()
	{
		l_Head = l_Head->p_Next;
		if (l_Head != nullptr)
			l_Head->p_Prev = nullptr;
	}

	void pop_back()
	{
		l_Tail = l_Tail->p_Prev;
		if (l_Tail != nullptr)
			l_Tail->p_Next = nullptr;
	}

	void insert(T& pos, T& item)
	{
		// Inserts before pos in the list
	 	if (pos.p_Prev != nullptr)
			pos.p_Prev->p_Next = &item;
		item.p_Next = &pos;
		item.p_Prev = pos.p_Prev;
		pos.p_Prev = &item;
		if (l_Head == &pos)
			l_Head = &item;
	}

	void remove(T& item)
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

	void clear()
	{
		l_Head = nullptr; l_Tail = nullptr;
	}

	bool empty() const
	{
		return l_Head == nullptr;
	}

	T& front()
	{
		return *begin();
	}

	const T& front() const
	{
		return *cbegin();
	}

	T& back()
	{
		return *end();
	}

	const T& back() const
	{
		return *cend();
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

	const_iterator cbegin()
	{
		return const_iterator(l_Head);
	}

	iterator end()
	{
		return iterator(nullptr);
	}

	const_iterator cend()
	{
		return const_iterator(nullptr);
	}

private:
	T* l_Head;
	T* l_Tail;
};

} // namespace util

#endif // ANANAS_UTIL_LIST_H
