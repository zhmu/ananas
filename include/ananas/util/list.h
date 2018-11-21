#ifndef ANANAS_UTIL_LIST_H
#define ANANAS_UTIL_LIST_H

namespace util {

template<typename T, typename Accessor> struct List;

namespace detail {

template<typename T, typename Accessor> using list_type = typename util::List<T, Accessor>;
template<typename T, typename NodeGetter> struct nodeptr_accessor;

// Pointers associated with each list entry
template<typename T>
struct list_node {
	list_node() = default;
	list_node(const list_node&) = delete;
	list_node(const list_node&&) = delete;
	list_node& operator=(const list_node&) = delete;
	list_node& operator=(list_node&&) = delete;

private:
	// Ideally, we'd befriend just the instances where T==U ...
	template<typename U, typename NodeGetter> friend class nodeptr_accessor;

	T* p_Prev = nullptr;
	T* p_Next = nullptr;
};

template<typename T, typename Accessor, typename Advance>
struct base_list_iterator
{
	typedef list_type<T, Accessor> ListType;

	constexpr base_list_iterator(const ListType& list, T* p)
	 : i_List(list), i_Ptr(p)
	{
	}

	constexpr base_list_iterator& operator++() {
		Advance::Next(i_List, i_Ptr);
		return *this;
	}

	constexpr base_list_iterator operator++(int) {
		auto s(*this);
		Advance::Next(i_List, i_Ptr);
		return s;
	}

	constexpr base_list_iterator& operator--() {
		Advance::Prev(i_List, i_Ptr);
		return *this;
	}

	constexpr base_list_iterator operator--(int) {
		auto s(*this);
		Advance::Prev(i_List, i_Ptr);
		return s;
	}

	constexpr T* operator->() const {
		return i_Ptr;
	}

	constexpr T& operator*() const {
		return *operator->();
	}

	constexpr bool operator==(const base_list_iterator& rhs) const {
		return i_Ptr == rhs.i_Ptr && &i_List == &rhs.i_List;
	}

	constexpr bool operator!=(const base_list_iterator& rhs) const {
		return !(*this == rhs);
	}

private:
	const ListType& i_List;
	T* i_Ptr;
};

template<typename T, typename Accessor>
struct forward_advance
{
	typedef list_type<T, Accessor> ListType;

	static constexpr void Next(const ListType& list, T*& p)
	{
		if (p == nullptr)
			p = list.l_Head;
		else
			p = Accessor::Next(*p);
	}

	static constexpr void Prev(const ListType& list, T*& p)
	{
		if (p == nullptr)
			p = list.l_Tail;
		else
			p = Accessor::Prev(*p);
	}
};

template<typename T, typename Accessor>
struct backward_advance
{
	typedef list_type<T, Accessor> ListType;

	static constexpr void Next(const ListType& list, T*& p)
	{
		forward_advance<T, Accessor>::Prev(list, p);
	}

	static constexpr void Prev(const ListType& list, T*& p)
	{
		forward_advance<T, Accessor>::Next(list, p);
	}
};

template<typename T, typename Accessor> using list_iterator = base_list_iterator<T, Accessor, forward_advance<T, Accessor>>;
template<typename T, typename Accessor> using list_reverse_iterator = base_list_iterator<T, Accessor, backward_advance<T, Accessor>>;

template<typename T, typename NodeGetter>
struct nodeptr_accessor
{
	static constexpr T*& Prev(T& t) {
		return NodeGetter::Get(t).p_Prev;
	}

	static constexpr T*& Next(T& t) {
		return NodeGetter::Get(t).p_Next;
	}
};

// Yields the member of struct NodePtr that is used for our pointers
template<typename T>
struct GetDerivedNodePtr
{
	static constexpr list_node<T>& Get(T& t) {
		return t.np_NodePtr;
	}
};

// Default accessor just uses the embedded NodePtr in each node, obtained by inheritance
template<typename T> using default_accessor = nodeptr_accessor<T, GetDerivedNodePtr<T> >;

}

/*
 * This implements a standard doubly-linked list structure; obtaining/removing
 * the head takes O(1), adding a new item takes O(1) and removing any item
 * takes O(1).  Iterating through the entire list takes O(n) (this also holds
 * for locating a single item).
 *
 * Each list has a 'head' and 'tail' element, and every item has a previous
 * and next pointer (contained in NodePtr) - you can either derive from NodePtr
 * or put Node's in your class and provide an accessor to them.
 */
template<typename T, typename Accessor = detail::default_accessor<T> >
struct List
{
	friend class detail::forward_advance<T, Accessor>;
	friend class detail::backward_advance<T, Accessor>;

	typedef typename detail::list_node<T> Node;

	struct NodePtr {
		detail::list_node<T> np_NodePtr;
	};

	typedef typename detail::list_iterator<T, Accessor> iterator;
	typedef typename detail::list_reverse_iterator<T, Accessor> reverse_iterator;
	template<typename NodeGetter> using nodeptr_accessor = detail::nodeptr_accessor<T, NodeGetter>;

	constexpr void push_back(T& item)
	{
		Accessor::Next(item) = nullptr;
		if (l_Head == nullptr) {
			Accessor::Prev(item) = nullptr;
			l_Head = &item;
		} else {
			Accessor::Prev(item) = l_Tail;
			Accessor::Next(*l_Tail) = &item;
		}
		l_Tail = &item;
	}

	constexpr void push_front(T& item)
	{
		Accessor::Prev(item) = nullptr;
		if (l_Head == nullptr) {
			Accessor::Next(item) = nullptr;
			l_Tail = &item;
		} else {
			Accessor::Next(item) = l_Head;
			Accessor::Prev(*l_Head) = &item;
		}
		l_Head = &item;
	}

	constexpr void pop_front()
	{
		l_Head = Accessor::Next(*l_Head);
		if (l_Head != nullptr)
			Accessor::Prev(*l_Head) = nullptr;
	}

	constexpr void pop_back()
	{
		l_Tail = Accessor::Prev(*l_Tail);
		if (l_Tail != nullptr)
			Accessor::Next(*l_Tail) = nullptr;
	}

	constexpr void insert(T& pos, T& item)
	{
		// Inserts before pos in the list
		if (Accessor::Prev(pos) != nullptr)
			Accessor::Next(*Accessor::Prev(pos)) = &item;
		Accessor::Next(item) = &pos;
		Accessor::Prev(item) = Accessor::Prev(pos);
		Accessor::Prev(pos) = &item;
		if (l_Head == &pos)
			l_Head = &item;
	}

	constexpr void remove(T& item)
	{
		if (Accessor::Prev(item) != nullptr)
			Accessor::Next(*Accessor::Prev(item)) = Accessor::Next(item);
		if (Accessor::Next(item) != nullptr)
			Accessor::Prev(*Accessor::Next(item)) = Accessor::Prev(item);
		if (l_Head == &item)
			l_Head = Accessor::Next(item);
		if (l_Tail == &item)
			l_Tail = Accessor::Prev(item);
	}

	constexpr void clear()
	{
		l_Head = nullptr;
		l_Tail = nullptr;
	}

	constexpr bool empty() const
	{
		return l_Head == nullptr;
	}

	constexpr T& front()
	{
		return *l_Head;
	}

	constexpr const T& front() const
	{
		return *l_Head;
	}

	constexpr T& back()
	{
		return *l_Tail;
	}

	constexpr const T& back() const
	{
		return *l_Tail;
	}

	constexpr List()
	 : l_Head(nullptr), l_Tail(nullptr)
	{
	}

	List(const List&) = delete;
	List(const List&&) = delete;
	List& operator=(const List&) = delete;
	List& operator=(List&&) = delete;

	constexpr iterator begin()
	{
		return iterator(*this, l_Head);
	}

	constexpr reverse_iterator rbegin()
	{
		return reverse_iterator(*this, l_Tail);
	}

	constexpr iterator end()
	{
		return iterator(*this, nullptr);
	}

	constexpr reverse_iterator rend()
	{
		return reverse_iterator(*this, nullptr);
	}

private:
	T* l_Head;
	T* l_Tail;
};

} // namespace util

#endif // ANANAS_UTIL_LIST_H
