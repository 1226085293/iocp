#pragma once
#include <atomic>
#include <memory>

template<class T>
class lock_free_stack {
private:
	struct node {
		std::shared_ptr<T> data;//shared_ptr管理好处：pop对象时如果用到拷贝函数可能会失败，存储指针可以提高性能
		node* next;
		//注意make_shared比直接shared_ptr构造的内存开销小
		node(const T& data_) : data(std::make_shared<T>(data_)), next(nullptr) {}
		node(T&& data_) : data(std::make_shared<T>(std::move(data_))), next(nullptr) {}
	};
	std::atomic<node*> _head, _tail;
	std::atomic<uint32_t> _size;
public:
	lock_free_stack();
	~lock_free_stack();

	std::shared_ptr<T> push(const T& data);
	std::shared_ptr<T> push(T&& data);
	std::shared_ptr<T> pop();
	template<class... Args>
	std::shared_ptr<T> emplace(Args&&... data);
	bool empty();

	std::shared_ptr<T> front();

	std::shared_ptr<T> back();

	uint32_t size();
};

#include "source/lock_free_stack.tcc"