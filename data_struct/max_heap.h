#pragma once
#include <cstdint>
#include <utility>

template <class T>
class max_heap {
private:
	struct node {
		uint32_t	size;
		T*			data;
	};

	node*		_heap;		//����
	uint32_t	_max_size;	//���洢��
	uint32_t	_size;		//�洢��

	// ����
	void expansion();
	// ɾ��ָ���±�ڵ�
	void del_node(uint32_t index_);

protected:
public:
	max_heap(uint32_t max_size_ = 64);
	~max_heap();
	// ��Ԫ��
	const node* begin();
	// ĩβԪ�غ�һλ
	const node* end();
	// ��ȡ���ֵ
	const node* front();
	// �������
	void push(uint32_t size_, T* data_);
	// ɾ�����ֵ
	T* pop();
	// ɾ��ָ��ֵ
	T* del(uint32_t size_);
	// ����ָ��ֵ
	T* find(uint32_t size_);
	// �Ƿ��
	bool empty();
	// ���
	void clear();
	// ��С
	uint32_t size();
};

#include "source/max_heap.tcc"