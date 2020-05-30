#pragma once
#include <queue>
#include <future>

class thread_pool {
private:
	bool								_death;		//����
	std::mutex							_mutex;		//������
	std::condition_variable				_condition;	//��������
	std::vector<std::thread>			_workers;	//�����߳�
	std::queue<std::function<void()>>	_tasks;		//�������
	static std::unique_ptr<thread_pool>	_instance;	//ʵ��ָ��
public:
	const uint8_t						thread_num;	//�߳���

	thread_pool();
	~thread_pool();
	// ����
	static thread_pool& instance();
	// �������
	template<class Func, class... Args>
	auto add(Func&& func, Args&& ... args);
};

#include "source/thread_pool.tcc"