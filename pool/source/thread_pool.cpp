#include "../thread_pool.h"
std::unique_ptr<thread_pool> thread_pool::_instance;

thread_pool& thread_pool::instance() {
	static std::once_flag s_flag;
	std::call_once(s_flag, [&]() {
		_instance.reset(new thread_pool);
		});
	return *_instance;
}

thread_pool::thread_pool() : _death(false), thread_num(2 * std::thread::hardware_concurrency()) {
	// 添加工作线程
	for (size_t i = 0; i < thread_num; ++i) {
		_workers.emplace_back(std::move([this]() {
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(_mutex);
					// 线程池关闭或者任务队列不为空则继续
					_condition.wait(lock, [this] {
						return _death || !_tasks.empty();
						}
					);
					// 线程池关闭并且任务队列为空退出线程
					if (_death && _tasks.empty()) {
						return;
					}
					task = move(_tasks.front());
					_tasks.pop();
				}
				// 执行任务
				task();
			}
			}
		));
	}
}

thread_pool::~thread_pool() {
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_death = true;
	}
	//唤醒所有线程
	_condition.notify_all();
	for (std::thread& worker : _workers) {
		worker.join();
	}
}