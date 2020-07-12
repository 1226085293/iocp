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
		node*		next;	//�¸��ڵ�
		uint32_t	size;	//��С

		node() = delete;
		node(const node&) = delete;
		node& operator =(const node&) = delete;
		~node() = delete;
	};
#pragma pack(pop)

	struct info {
		node*	block;	//��������
		node*	free;	//���нڵ�

		info();
		info(const info&) = default;
		info& operator =(const info&) = delete;
		~info();
	};

#if (LOCK_MODEL == LOCK)
	std::mutex			_mutex;			//������
#elif (LOCK_MODEL == CRITICAL)
	CRITICAL_SECTION	_cri;			//�ٽ���
#endif
	info*				_all_memory;	//�����ڴ漯��

	// ��ȡ�ڴ油���С
	uint32_t memory_pading(char* ptr, uint32_t align);
	// ���������ڴ�
	void allocate_block(uint32_t power_);
protected:
public:
	variable_memory_pool();
	variable_memory_pool(const variable_memory_pool&) = delete;
	variable_memory_pool& operator=(const variable_memory_pool&) = delete;
	~variable_memory_pool();

	// ����ڵ�
	char* allocate(uint32_t size_);
	// ���ն���
	void deallocate(char* ptr_);
	// ��������
	template <class T>
	T* new_obj(uint32_t size_);

	template <class T, class... Args>
	T* new_obj(uint32_t size_, Args&&... args);
	// ɾ������
	template <class T>
	void del_obj(T* obj_ptr);
	// ��ʼ������
	template <class T>
	void construct(T* obj_ptr);

	template <class T>
	void construct(T* obj_ptr, const T& that);

	template <class T, class... Args>
	void construct(T* obj_ptr, Args&&... args);
	// ���ٶ���
	template <class T>
	void destroy(T* obj_ptr);
};

#include "source/variable_memory_pool.tcc"