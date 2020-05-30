#pragma once
#include <cstdint>
#include "raii/critical.h"

template <class T, uint32_t block_size = 512>
class fixed_memory_pool {
    static_assert(block_size >= 2, "block_size too small.");
private:
    union node {       //备注: 使用union更节省内存但是必须保证在deallocate后不再改动此内存块,否则将造成严重后果
		T obj;
		node* next;

        node() = delete;
        node(const node&) = delete;
        node& operator =(const node&) = delete;
        ~node() = delete;
	};

    CRITICAL_SECTION    _cri;           //临界区
    uint32_t            _block_size;    //区块大小
	node*               _current_block; //当前区块
	node*               _current_node;  //当前节点
	node*               _last_node;     //最后节点
	node*               _free_node;     //空闲节点

    // 获取内存填充
	uint32_t memory_pading(char* ptr, uint32_t align) const;
    // 分配区块内存
	void allocate_block();
protected:
public:
	fixed_memory_pool(bool allocate = false);
    fixed_memory_pool(const fixed_memory_pool&) = delete;
	fixed_memory_pool& operator=(const fixed_memory_pool& fixed_memory_pool) = delete;
	~fixed_memory_pool();

    // 分配节点
	T* allocate();
    // 分配空闲节点
    T* allocate_free();
    // 回收对象
	void deallocate(T* obj_ptr);
    // 获取存在的节点数量
	uint32_t max_size() const;
    // 创建对象
    T* new_obj();

    template <class... Args>
    T* new_obj(Args&&... args);
    // 删除对象
    void del_obj(T* obj_ptr);
    // 初始化对象
    void construct(T* obj_ptr);

    void construct(T* obj_ptr, const T& that);

	template <class... Args>
	void construct(T* obj_ptr, Args&&... args);
    // 销毁对象
	void destroy(T* obj_ptr);
};

#include "source/fixed_memory_pool.tcc"