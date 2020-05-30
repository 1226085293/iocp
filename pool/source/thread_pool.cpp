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
	// ��ӹ����߳�
	for (size_t i = 0; i < thread_num; ++i) {
		_workers.emplace_back(std::move([this]() {
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(_mutex);
					// �̳߳عرջ���������в�Ϊ�������
					_condition.wait(lock, [this] {
						return _death || !_tasks.empty();
						}
					);
					// �̳߳عرղ����������Ϊ���˳��߳�
					if (_death && _tasks.empty()) {
						return;
					}
					task = move(_tasks.front());
					_tasks.pop();
				}
				// ִ������
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
	//���������߳�
	_condition.notify_all();
	for (std::thread& worker : _workers) {
		worker.join();
	}
}