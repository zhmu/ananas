#ifndef ANANAS_UTIL_LIST_H
#define ANANAS_UTIL_LIST_H

namespace util {

template<typename T> struct List;

namespace detail {

template<typename T> using list_type = typename util::List<T>;

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

template<typename T, typename Advance>
struct base_list_iterator
{
	typedef list_type<T> ListType;
	typedef list_node<T> Node;

	base_list_iterator(const ListType& list, Node* p)
	 : i_List(list), i_NodePtr(p)
	{
	}
	const ListType& i_List;
	Node* i_NodePtr;

	base_list_iterator& operator++() {
		Advance::Next(i_List, i_NodePtr);
		return *this;
	}

	base_list_iterator operator++(int) {
		auto s(*this);
		Advance::Next(i_List, i_NodePtr);
		return s;
	}

	base_list_iterator& operator--() {
		Advance::Prev(i_List, i_NodePtr);
		return *this;
	}

	base_list_iterator operator--(int) {
		auto s(*this);
		Advance::Prev(i_List, i_NodePtr);
		return s;
	}

	T* operator->() const {
		return static_cast<T*>(i_NodePtr);
	}

	T& operator*() const {
		return *operator->();
	}

	bool operator==(const base_list_iterator& rhs) const {
		return i_NodePtr == rhs.i_NodePtr && &i_List == &rhs.i_List;
	}

	bool operator!=(const base_list_iterator& rhs) const {
		return !(*this == rhs);
	}
};

template<typename T>
struct forward_advance
{
	typedef list_type<T> ListType;
	typedef list_node<T> Node;

	static void Next(const ListType& list, Node*& p)
	{
		if (p == nullptr)
			p = list.l_Head;
		else
			p = p->p_Next;
	}

	static void Prev(const ListType& list, Node*& p)
	{
		if (p == nullptr)
			p = list.l_Tail;
		else
			p = p->p_Prev;
	}
};

template<typename T>
struct backward_advance
{
	typedef list_type<T> ListType;
	typedef list_node<T> Node;

	static void Next(const ListType& list, Node*& p)
	{
		forward_advance<T>::Prev(list, p);
	}

	static void Prev(const ListType& list, Node*& p)
	{
		forward_advance<T>::Next(list, p);
	}
};

template<typename T> using list_iterator = base_list_iterator<T, forward_advance<T>>;
template<typename T> using list_reverse_iterator = base_list_iterator<T, backward_advance<T>>;

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
	friend class detail::forward_advance<T>;
	friend class detail::backward_advance<T>;

	typedef typename detail::list_node<T> NodePtr;
	typedef typename detail::list_iterator<T> iterator;
	typedef typename detail::list_reverse_iterator<T> reverse_iterator;

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
		return *l_Head;
	}

	const T& front() const
	{
		return *l_Head;
	}

	T& back()
	{
		return *l_Tail;
	}

	const T& back() const
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
		return iterator(*this, l_Head);
	}

	reverse_iterator rbegin()
	{
		return reverse_iterator(*this, l_Tail);
	}

	iterator end()
	{
		return iterator(*this, nullptr);
	}

	reverse_iterator rend()
	{
		return reverse_iterator(*this, nullptr);
	}

private:
	T* l_Head;
	T* l_Tail;
};

} // namespace util

#endif // ANANAS_UTIL_LIST_H
