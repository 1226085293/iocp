#pragma once
#include <cstdint>
#include <utility>

template <class T>
class mini_heap {
private:
	struct node {
		uint32_t	size;
		T*			data;
	};

	node*		_heap;		//最小堆
	uint32_t	_max_size;	//最大存储数
	uint32_t	_size;		//存储数

	// 扩容
	void expansion();
	// 删除指定下标节点
	void del_node(uint32_t index_);
protected:
public:
	mini_heap(uint32_t max_size_ = 64);
	~mini_heap();
	// 首元素
	const node* begin();
	// 末尾元素后一位
	const node* end();
	// 获取最小值
	const node* front();
	// 添加数据
	void push(uint32_t size_, T* data_);
	// 删除最小值
	T* pop();
	// 删除指定值
	T* del(uint32_t size_);
	// 查找指定值
	T* find(uint32_t size_);
	// 是否空
	bool empty();
	// 清空
	void clear();
	// 大小
	uint32_t size();
};

#include "source/mini_heap.tcc"