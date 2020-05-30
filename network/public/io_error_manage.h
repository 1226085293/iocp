#pragma once
#include <future>
#include <functional>
#include "../public/define.h"

namespace network {
	// 数据包错误信息
	class io_error_manage {
	private:
		uint8_t					error_count[io_err_tnum];				//错误计数
		uint8_t					continued_error_count[io_err_tnum];		//连续错误计数
		uint8_t					error_limit[io_err_tnum];				//错误限制
		uint8_t					continued_error_limit[io_err_tnum];		//连续错误限制
		std::function<void()>	error_process[io_err_tnum];				//错误处理函数
		std::function<void()>	continued_error_process[io_err_tnum];	//连续错误处理函数
	protected:
	public:
		io_error_manage();
		io_error_manage(const io_error_manage&) = default;
		io_error_manage& operator =(const io_error_manage&) = default;
		~io_error_manage() = default;

		// 增加错误计数
		void add_error(io_err_type type_);
		// 无错误(清除连续错误计数)
		void no_error(io_err_type type_);
		// 设置错误回调函数
		template<class Func, class... Args>
		void set_error_process(io_err_type type_, uint8_t limit_num_, Func&& func, Args&& ... args);
		// 设置连续错误回调函数
		template<class Func, class... Args>
		void set_continued_error_process(io_err_type type_, uint8_t limit_num_, Func&& func, Args&& ... args);
	};

#include "source/io_error_manage.icc"
}