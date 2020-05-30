template<class Func, class... Args>
auto thread_pool::add(Func&& func, Args&&... args) {
	using re_type = typename std::invoke_result<Func, Args...>::type;
	// �����Ѱ�װ�ĺ�������ָ��
	auto task = std::make_shared<std::packaged_task<re_type()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
	std::future<re_type> future = task->get_future();   //��ȡfuture�������task��״̬��Ϊready����������ǰ������
	{
		// �������
		std::lock_guard<std::mutex> lock(_mutex);
		if (_death) {
			throw std::runtime_error("threadPool close");
		}
		_tasks.emplace(std::move([task] {
			(*task)();
			})
		);
	}
	// ���ѵ����߳�ִ������
	_condition.notify_one();
	return future;
}