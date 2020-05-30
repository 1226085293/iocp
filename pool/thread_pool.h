#pragma once
#include <queue>
#include <future>

class thread_pool {
private:
	bool								_death;		//结束
	std::mutex							_mutex;		//互斥锁
	std::condition_variable				_condition;	//条件变量
	std::vector<std::thread>			_workers;	//工作线程
	std::queue<std::function<void()>>	_tasks;		//任务队列
	static std::unique_ptr<thread_pool>	_instance;	//实例指针
public:
	const uint8_t						thread_num;	//线程数

	thread_pool();
	~thread_pool();
	// 单例
	static thread_pool& instance();
	// 添加任务
	template<class Func, class... Args>
	auto add(Func&& func, Args&& ... args);
};

#include "source/thread_pool.tcc"