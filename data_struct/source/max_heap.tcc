template <class T>
max_heap<T>::max_heap(uint32_t max_size_) :
	_heap(new node[max_size_]{}),
	_max_size(max_size_),
	_size(0)
{}

template <class T>
max_heap<T>::~max_heap() {
	delete[] _heap;
}

template <class T>
void max_heap<T>::expansion() {
	auto size = sizeof(node) * _max_size;
	node* new_heap = new node[_max_size <<= 1]{};
	memcpy_s(new_heap, size << 1, _heap, size);
	delete[] _heap;
	_heap = new_heap;
}

template <class T>
void max_heap<T>::del_node(uint32_t index_) {
	if (_size < index_) {
		return;
	}
	// 向下调整
	if (_heap[_size].size < _heap[index_].size) {
		uint32_t index1 = index_, index2 = index1 << 1;
		while (index2 < _size) {
			if (_heap[index2].size < _heap[index2 + 1].size) {
				++index2;
			}
			if (_heap[_size].size < _heap[index2].size) {
				_heap[index1] = _heap[index2];
				index1 = index2;
				index2 <<= 1;
			}
			else {
				break;
			}
		}
		_heap[index1] = _heap[_size];
		memset(&_heap[_size], 0, sizeof(node));
	}
	// 向上调整
	else if (_heap[_size].size > _heap[index_].size) {
		uint32_t index1 = index_ >> 1, index2 = _size;
		while (_heap[index1].size < _heap[index2].size) {
			std::swap(_heap[index1], _heap[_size]);
			index2 = index1;
			index1 >>= 1;
		}
		_heap[index_] = _heap[_size];
		memset(&_heap[_size], 0, sizeof(node));
	}
	// 不做调整
	else {
		_heap[index_] = _heap[_size];
		memset(&_heap[_size], 0, sizeof(node));
	}
	--_size;
}

template <class T>
inline const typename max_heap<T>::node* max_heap<T>::begin() {
	return _heap + 1;
}

template <class T>
inline const typename max_heap<T>::node* max_heap<T>::end() {
	return _heap + _size + 1;
}

template <class T>
inline const typename max_heap<T>::node* max_heap<T>::front() {
	if (_size < 1) {
		return nullptr;
	}
	return _heap + 1;
}

template <class T>
void max_heap<T>::push(uint32_t size_, T* data_) {
	if (!data_) {
		return;
	}
	if (++_size == _max_size) {
		expansion();
	}
	_heap[_size].size = size_;
	_heap[_size].data = data_;
	uint32_t index1 = _size, index2 = index1 >> 1;
	while (index1 > 1 && _heap[index2].size < _heap[index1].size) {
		std::swap(_heap[index1], _heap[index2]);
		index1 = index2;
		index2 >>= 1;
	}
}

template <class T>
inline T* max_heap<T>::pop() {
	if (_size < 1) {
		return nullptr;
	}
	T* data = _heap[1].data;
	del_node(1);
	return data;
}

template <class T>
T* max_heap<T>::del(uint32_t size_) {
	if (_size < 1) {
		return nullptr;
	}
	// push了key值相同的复数数据可能不是想要的结果
	uint32_t index1 = 0;
	for (uint32_t i = 1; i <= _size; ++i) {
		if (_heap[i].size == size_) {
			index1 = i;
			break;
		}
	}
	if (index1) {
		T* data = _heap[index1].data;
		del_node(index1);
		return data;
	}
	return nullptr;
}

template <class T>
T* max_heap<T>::find(uint32_t size_) {
	uint32_t index1 = 0;
	for (uint32_t i = 1; i <= _size; ++i) {
		if (_heap[i].size == size_) {
			index1 = i;
			break;
		}
	}
	if (!index1) {
		return nullptr;
	}
	return _heap[index1].data;
}

template <class T>
inline bool max_heap<T>::empty() {
	return _size == 0;
}

template <class T>
inline void max_heap<T>::clear() {
	while (!empty()) {
		pop();
	}
}

template <class T>
inline uint32_t max_heap<T>::size() {
	return _size;
}