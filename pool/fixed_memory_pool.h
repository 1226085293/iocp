#pragma once
#include <cstdint>
#include "raii/critical.h"

template <class T, uint32_t block_size = 512>
class fixed_memory_pool {
    static_assert(block_size >= 2, "block_size too small.");
private:
    union node {       //��ע: ʹ��union����ʡ�ڴ浫�Ǳ��뱣֤��deallocate���ٸĶ����ڴ��,����������غ��
		T obj;
		node* next;

        node() = delete;
        node(const node&) = delete;
        node& operator =(const node&) = delete;
        ~node() = delete;
	};

    CRITICAL_SECTION    _cri;           //�ٽ���
    uint32_t            _block_size;    //�����С
	node*               _current_block; //��ǰ����
	node*               _current_node;  //��ǰ�ڵ�
	node*               _last_node;     //���ڵ�
	node*               _free_node;     //���нڵ�

    // ��ȡ�ڴ����
	uint32_t memory_pading(char* ptr, uint32_t align) const;
    // ���������ڴ�
	void allocate_block();
protected:
public:
	fixed_memory_pool(bool allocate = false);
    fixed_memory_pool(const fixed_memory_pool&) = delete;
	fixed_memory_pool& operator=(const fixed_memory_pool& fixed_memory_pool) = delete;
	~fixed_memory_pool();

    // ����ڵ�
	T* allocate();
    // ������нڵ�
    T* allocate_free();
    // ���ն���
	void deallocate(T* obj_ptr);
    // ��ȡ���ڵĽڵ�����
	uint32_t max_size() const;
    // ��������
    T* new_obj();

    template <class... Args>
    T* new_obj(Args&&... args);
    // ɾ������
    void del_obj(T* obj_ptr);
    // ��ʼ������
    void construct(T* obj_ptr);

    void construct(T* obj_ptr, const T& that);

	template <class... Args>
	void construct(T* obj_ptr, Args&&... args);
    // ���ٶ���
	void destroy(T* obj_ptr);
};

#include "source/fixed_memory_pool.tcc"