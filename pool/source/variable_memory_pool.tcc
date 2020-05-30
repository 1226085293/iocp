template <uint16_t min_memory, uint32_t block_contain>
inline variable_memory_pool<min_memory, block_contain>::info::info() : block(nullptr), free(nullptr) {}

template <uint16_t min_memory, uint32_t block_contain>
variable_memory_pool<min_memory, block_contain>::info::~info() {
	node* next;
	while (block) {
		next = block->next;
		std::free(block);
		block = next;
	}
}

template <uint16_t min_memory, uint32_t block_contain>
variable_memory_pool<min_memory, block_contain>::variable_memory_pool() {
#if (LOCK_MODEL == CRITICAL)
	InitializeCriticalSection(&_cri);
#endif
	_all_memory = new info[tool::math::power::similar_power(UINT32_MAX)];
}

template <uint16_t min_memory, uint32_t block_contain>
variable_memory_pool<min_memory, block_contain>::~variable_memory_pool() {
#if (LOCK_MODEL == CRITICAL)
	DeleteCriticalSection(&_cri);
#endif
	delete[] _all_memory;
}

template <uint16_t min_memory, uint32_t block_contain>
inline uint32_t variable_memory_pool<min_memory, block_contain>::memory_pading(char* ptr, uint32_t align) {
	// 内存补齐的地址为成员大小的整数倍 (alignof(node) - reinterpret_cast<uintptr_t>(ptr) % alignof(node)) % alignof(node) = (alignof(node) - result) % alignof(node)
	return ((align - reinterpret_cast<uintptr_t>(ptr)) % align);
}

template <uint16_t min_memory, uint32_t block_contain>
void variable_memory_pool<min_memory, block_contain>::allocate_block(uint32_t power_) {
#if (LOCK_MODEL == LOCK)
	std::lock_guard<std::mutex> lock(_mutex);
#elif (LOCK_MODEL == CRITICAL)
	raii::critical r1(&_cri);
#endif
	auto& block_set = _all_memory[power_];
	// 分配内存大小 = sizeof(node*) + (占用空间 + sizeof(node)) * block_contain + 可能出现的内存补齐大小
	uint32_t mem_size = tool::math::power::get_power(power_);
	char* new_block = new char[sizeof(node*) + (mem_size + sizeof(node)) * block_contain + sizeof(node)];
	// 添加到区块
	reinterpret_cast<node*>(new_block)->next = block_set.block;
	block_set.block = reinterpret_cast<node*>(new_block);
	// 内存补齐
	char* body = new_block + sizeof(node*);
	auto pad_size = memory_pading(body, alignof(node));
	body += pad_size;
	// 链接节点
	node* head = reinterpret_cast<node*>(body);
	for (int i = 0; i < block_contain; ++i) {
		head->size = mem_size;
		head->next = block_set.free;
		block_set.free = head;
		head = reinterpret_cast<node*>(reinterpret_cast<char*>(head + 1) + mem_size);
	}
}

template <uint16_t min_memory, uint32_t block_contain>
char* variable_memory_pool<min_memory, block_contain>::allocate(uint32_t size_) {
#if (LOCK_MODEL == LOCK)
	std::lock_guard<std::mutex> lock(_mutex);
#elif (LOCK_MODEL == CRITICAL)
	raii::critical r1(&_cri);
#endif
	// 计算需要的内存容量次方
	if (size_ < min_memory) {
		size_ = min_memory;
	}
	auto power = tool::math::power::similar_power(size_) + (tool::math::power::is_power(size_) ? 0 : 1);
	// 若无空闲节点重新创建
	if (!_all_memory[power].free) {
		allocate_block(power);
	}
	char* memory = reinterpret_cast<char*>(_all_memory[power].free + 1);
	_all_memory[power].free = _all_memory[power].free->next;
	return memory;
}

template <uint16_t min_memory, uint32_t block_contain>
void variable_memory_pool<min_memory, block_contain>::deallocate(char* ptr_) {
#if (LOCK_MODEL == LOCK)
	std::lock_guard<std::mutex> lock(_mutex);
#elif (LOCK_MODEL == CRITICAL)
	raii::critical r1(&_cri);
#endif
	if (!ptr_) {
		return;
	}
	node* dec_node = reinterpret_cast<node*>(ptr_ - sizeof(node));
	uint32_t power = tool::math::power::similar_power(dec_node->size);
	if (dec_node->size < min_memory || !tool::math::power::is_power(dec_node->size)) {
		printf("无效地址, 拒绝回收\n");
		return;
	}
	auto& block_set = _all_memory[power];
	dec_node->next = block_set.free;
	block_set.free = dec_node;
}

template <uint16_t min_memory, uint32_t block_contain>
template <class T>
inline T* variable_memory_pool<min_memory, block_contain>::new_obj(uint32_t size_) {
	T* result = reinterpret_cast<T*>(allocate(size_));
	construct(result);
	return result;
}

template <uint16_t min_memory, uint32_t block_contain>
template <class T, class... Args>
inline T* variable_memory_pool<min_memory, block_contain>::new_obj(uint32_t size_, Args&&... args) {
	T* result = reinterpret_cast<T*>(allocate(size_));
	new (result) T(std::forward<Args>(args)...);
	return result;
}

template <uint16_t min_memory, uint32_t block_contain>
template <class T>
inline void variable_memory_pool<min_memory, block_contain>::del_obj(T* obj_ptr) {
	if (obj_ptr) {
		obj_ptr->~T();
		deallocate(obj_ptr);
	}
}

template <uint16_t min_memory, uint32_t block_contain>
template <class T>
inline void variable_memory_pool<min_memory, block_contain>::construct(T* obj_ptr) {
	new (obj_ptr) T();
}

template <uint16_t min_memory, uint32_t block_contain>
template <class T>
inline void variable_memory_pool<min_memory, block_contain>::construct(T* obj_ptr, const T& that) {
	*obj_ptr = that;
}

template <uint16_t min_memory, uint32_t block_contain>
template <class T, class... Args>
inline void variable_memory_pool<min_memory, block_contain>::construct(T* obj_ptr, Args&&... args) {
	new (obj_ptr) T(std::forward<Args>(args)...);
}

template <uint16_t min_memory, uint32_t block_contain>
template <class T>
inline void variable_memory_pool<min_memory, block_contain>::destroy(T* obj_ptr) {
	obj_ptr->~T();
}