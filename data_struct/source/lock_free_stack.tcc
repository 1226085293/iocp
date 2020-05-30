template<class T>
lock_free_stack<T>::lock_free_stack() :_head(nullptr), _tail(nullptr), _size(0) {}

template<class T>
lock_free_stack<T>::~lock_free_stack() {
	_head = nullptr;
}

template<class T>
inline std::shared_ptr<T> lock_free_stack<T>::push(const T& data) {
	return emplace(data);
}

template<class T>
inline std::shared_ptr<T> lock_free_stack<T>::push(T&& data) {
	return emplace(std::move(data));
}

template<class T>
std::shared_ptr<T> lock_free_stack<T>::pop() {
	if (_size == 0) {
		return nullptr;
	}
	auto old__head = _head.load();//拿住栈顶元素，但是可能后续被更新，更新发生在_head!=old__head时
	while (old__head && !_head.compare_exchange_weak(old__head, old__head->next, std::memory_order_release, std::memory_order_relaxed));//这里注意首先要先判断old__head是否为nullptr防止操作空链表，然后按照compare_exchange_weak语义更新链表头结点。若_head==old__head则更新_head为old__head->next并返回true，结束循环，删除栈顶元素成功;若_head!=old__head表明在此期间有其它线程操作了_head，因此更新old__head为新的_head,返回false进入下一轮循环，直至删除成功。
	if (--_size == 0) {
		_head = nullptr;
	}
	return old__head ? old__head->data : nullptr;
}

template<class T>
template<class... Args>
std::shared_ptr<T> lock_free_stack<T>::emplace(Args&&... data) {
	node* const new_node = new node(std::forward<Args>(data)...);
	new_node->next = _head;//每次从链表头插入
	if (empty()) {
		_tail = new_node;
	}
	while (!_head.compare_exchange_weak(new_node->next, new_node));//若_head==new_node->next则更新_head为new_node,返回true结束循环，插入成功; 若_head!=new_node->next表明有其它线程在此期间对_head操作了，将new_node->next更新为新的_head，返回false，继续进入下一次while循环。这里采用atomic::compare_exchange_weak比atomic::compare_exchange_strong快，因为compare_exchange_weak可能在元素相等的时候返回false所以适合在循环中，而atomic::compare_exchange_strong保证了比较的正确性，不适合用于循环
	++_size;
	return new_node->data;
}

template<class T>
inline bool lock_free_stack<T>::empty() {
	return _size == 0;
}

template<class T>
inline std::shared_ptr<T> lock_free_stack<T>::front() {
	return _head.load()->data;
}

template<class T>
inline std::shared_ptr<T> lock_free_stack<T>::back() {
	return _tail.load()->data;
}

template<class T>
inline uint32_t lock_free_stack<T>::size() {
	return _size;
}