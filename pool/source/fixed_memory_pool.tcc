template <class T, uint32_t block_size>
fixed_memory_pool<T, block_size>::fixed_memory_pool(bool allocate) :
    _block_size(block_size * sizeof(node) + sizeof(node*)),
    _current_block(nullptr),
    _current_node(nullptr),
    _last_node(nullptr),
    _free_node(nullptr)
{
    InitializeCriticalSection(&_cri);
    if (allocate) {
        allocate_block();
    }
}

template <class T, uint32_t block_size>
fixed_memory_pool<T, block_size>::~fixed_memory_pool() {
    {
        raii::critical r1(&_cri);
        node* block = _current_block, * next;
        while (block != nullptr) {
            next = block->next;
            free(block);
            block = next;
        }
    }
    DeleteCriticalSection(&_cri);
}

template <class T, uint32_t block_size>
inline uint32_t fixed_memory_pool<T, block_size>::memory_pading(char* ptr, uint32_t align) const {
    // 内存补齐的地址为成员大小的整数倍 (alignof(node) - reinterpret_cast<uintptr_t>(ptr) % alignof(node)) % alignof(node) = (alignof(node) - result) % alignof(node)
    return ((align - reinterpret_cast<uintptr_t>(ptr)) % align);
}

template <class T, uint32_t block_size>
void fixed_memory_pool<T, block_size>::allocate_block() {
    raii::critical r1(&_cri);
    char* new_block = new char[_block_size];
    (reinterpret_cast<node*>(new_block))->next = _current_block;
    _current_block = reinterpret_cast<node*>(new_block);
    char* body = new_block + sizeof(node*);
    uint32_t pad_size = memory_pading(body, alignof(node));
    // body + 内存补齐大小 = _current_node
    _current_node = reinterpret_cast<node*>(body + pad_size);
    _last_node = reinterpret_cast<node*>(new_block + _block_size - sizeof(node) + 1);
}

template <class T, uint32_t block_size>
T* fixed_memory_pool<T, block_size>::allocate() {
    raii::critical r1(&_cri);
    T* result = allocate_free();
    if (!result) {
        if (_current_node >= _last_node) {
            allocate_block();
        }
        return reinterpret_cast<T*>(_current_node++);
    }
    return result;
}

template <class T, uint32_t block_size>
T* fixed_memory_pool<T, block_size>::allocate_free() {
    raii::critical r1(&_cri);
    if (_free_node) {
        //printf("取出内存：%I64d\n", _free_node);
        T* result = reinterpret_cast<T*>(_free_node);
        _free_node = _free_node->next;
        return result;
    }
    return nullptr;
}

template <class T, uint32_t block_size>
void fixed_memory_pool<T, block_size>::deallocate(T* obj_ptr) {
    raii::critical r1(&_cri);
    if (obj_ptr) {
        //printf("释放内存：%I64d\n", obj_ptr);
        memset(obj_ptr, 0, sizeof(T));
        ((node*)(obj_ptr))->next = _free_node;
        _free_node = ((node*)(obj_ptr));
    }
}

template <class T, uint32_t block_size>
uint32_t fixed_memory_pool<T, block_size>::max_size() const {
    raii::critical r1(&_cri);
    uint32_t max_blocks(0);
    node* block_node = _current_block;
    while (block_node != nullptr) {
        ++max_blocks;
        block_node = block_node->next;
    }
    return max_blocks ? (_block_size - sizeof(node*)) / sizeof(node) * max_blocks : 0;
}

template <class T, uint32_t block_size>
inline T* fixed_memory_pool<T, block_size>::new_obj() {
    T* result = allocate();
    construct(result);
    return result;
}

template <class T, uint32_t block_size>
template <class... Args>
inline T* fixed_memory_pool<T, block_size>::new_obj(Args&&... args) {
    T* result = allocate();
    new (result) T(std::forward<Args>(args)...);
    return result;
}

template <class T, uint32_t block_size>
inline void fixed_memory_pool<T, block_size>::del_obj(T* obj_ptr) {
    if (obj_ptr) {
        obj_ptr->~T();
        deallocate(obj_ptr);
    }
}

template <class T, uint32_t block_size>
inline void fixed_memory_pool<T, block_size>::construct(T* obj_ptr) {
    new (obj_ptr) T();
}

template <class T, uint32_t block_size>
inline void fixed_memory_pool<T, block_size>::construct(T* obj_ptr, const T& that) {
    *obj_ptr = that;
}