#pragma once
#include <cstdint>
#include <vector>
#include "tool/math.h"

#define LOCK 0x01
#define CRITICAL 0x02
#define LOCK_MODEL CRITICAL

#if (LOCK_MODEL == CRITICAL)
#include "other/raii/critical.h"
#elif (LOCK_MODEL == LOCK)
#include <mutex>
#else
#endif

template <uint16_t min_memory = 16, uint32_t block_contain = 10>
class variable_memory_pool {
	static_assert(min_memory > 0, "min_memory too small.");
	static_assert(block_contain > 0, "contain_size too small.");
private:
#pragma pack(push)
#pragma pack(4)
	struct node {
		node*		next;	//下个节点
		uint32_t	size;	//大小

		node() = delete;
		node(const node&) = delete;
		node& operator =(const node&) = delete;
		~node() = delete;
	};
#pragma pack(pop)

	struct info {
		node*	block;	//所有区块
		node*	free;	//空闲节点

		info();
		info(const info&) = default;
		info& operator =(const info&) = delete;
		~info();
	};

#if (LOCK_MODEL == LOCK)
	std::mutex			_mutex;			//互斥锁
#elif (LOCK_MODEL == CRITICAL)
	CRITICAL_SECTION	_cri;			//临界区
#endif
	info*				_all_memory;	//所有内存集合

	// 获取内存补齐大小
	uint32_t memory_pading(char* ptr, uint32_t align);
	// 分配区块内存
	void allocate_block(uint32_t power_);
protected:
public:
	variable_memory_pool();
	variable_memory_pool(const variable_memory_pool&) = delete;
	variable_memory_pool& operator=(const variable_memory_pool&) = delete;
	~variable_memory_pool();

	// 分配节点
	char* allocate(uint32_t size_);
	// 回收对象
	void deallocate(char* ptr_);
	// 创建对象
	template <class T>
	T* new_obj(uint32_t size_);

	template <class T, class... Args>
	T* new_obj(uint32_t size_, Args&&... args);
	// 删除对象
	template <class T>
	void del_obj(T* obj_ptr);
	// 初始化对象
	template <class T>
	void construct(T* obj_ptr);

	template <class T>
	void construct(T* obj_ptr, const T& that);

	template <class T, class... Args>
	void construct(T* obj_ptr, Args&&... args);
	// 销毁对象
	template <class T>
	void destroy(T* obj_ptr);
};

#include "source/variable_memory_pool.tcc"