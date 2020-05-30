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
	auto old__head = _head.load();//��סջ��Ԫ�أ����ǿ��ܺ��������£����·�����_head!=old__headʱ
	while (old__head && !_head.compare_exchange_weak(old__head, old__head->next, std::memory_order_release, std::memory_order_relaxed));//����ע������Ҫ���ж�old__head�Ƿ�Ϊnullptr��ֹ����������Ȼ����compare_exchange_weak�����������ͷ��㡣��_head==old__head�����_headΪold__head->next������true������ѭ����ɾ��ջ��Ԫ�سɹ�;��_head!=old__head�����ڴ��ڼ��������̲߳�����_head����˸���old__headΪ�µ�_head,����false������һ��ѭ����ֱ��ɾ���ɹ���
	if (--_size == 0) {
		_head = nullptr;
	}
	return old__head ? old__head->data : nullptr;
}

template<class T>
template<class... Args>
std::shared_ptr<T> lock_free_stack<T>::emplace(Args&&... data) {
	node* const new_node = new node(std::forward<Args>(data)...);
	new_node->next = _head;//ÿ�δ�����ͷ����
	if (empty()) {
		_tail = new_node;
	}
	while (!_head.compare_exchange_weak(new_node->next, new_node));//��_head==new_node->next�����_headΪnew_node,����true����ѭ��������ɹ�; ��_head!=new_node->next�����������߳��ڴ��ڼ��_head�����ˣ���new_node->next����Ϊ�µ�_head������false������������һ��whileѭ�����������atomic::compare_exchange_weak��atomic::compare_exchange_strong�죬��Ϊcompare_exchange_weak������Ԫ����ȵ�ʱ�򷵻�false�����ʺ���ѭ���У���atomic::compare_exchange_strong��֤�˱Ƚϵ���ȷ�ԣ����ʺ�����ѭ��
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