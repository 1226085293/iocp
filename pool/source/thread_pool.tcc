template<class Func, class... Args>
auto thread_pool::add(Func&& func, Args&&... args) {
	using re_type = typename std::invoke_result<Func, Args...>::type;
	// 返回已包装的函数对象指针
	auto task = std::make_shared<std::packaged_task<re_type()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
	std::future<re_type> future = task->get_future();   //获取future对象，如果task的状态不为ready，会阻塞当前调用者
	{
		// 添加任务
		std::lock_guard<std::mutex> lock(_mutex);
		if (_death) {
			throw std::runtime_error("threadPool close");
		}
		_tasks.emplace(std::move([task] {
			(*task)();
			})
		);
	}
	// 唤醒单个线程执行任务
	_condition.notify_one();
	return future;
}