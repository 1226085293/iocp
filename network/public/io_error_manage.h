#pragma once
#include <future>
#include <functional>
#include "../public/define.h"

namespace network {
	// ���ݰ�������Ϣ
	class io_error_manage {
	private:
		uint8_t					error_count[io_err_tnum];				//�������
		uint8_t					continued_error_count[io_err_tnum];		//�����������
		uint8_t					error_limit[io_err_tnum];				//��������
		uint8_t					continued_error_limit[io_err_tnum];		//������������
		std::function<void()>	error_process[io_err_tnum];				//��������
		std::function<void()>	continued_error_process[io_err_tnum];	//������������
	protected:
	public:
		io_error_manage();
		io_error_manage(const io_error_manage&) = default;
		io_error_manage& operator =(const io_error_manage&) = default;
		~io_error_manage() = default;

		// ���Ӵ������
		void add_error(io_err_type type_);
		// �޴���(��������������)
		void no_error(io_err_type type_);
		// ���ô���ص�����
		template<class Func, class... Args>
		void set_error_process(io_err_type type_, uint8_t limit_num_, Func&& func, Args&& ... args);
		// ������������ص�����
		template<class Func, class... Args>
		void set_continued_error_process(io_err_type type_, uint8_t limit_num_, Func&& func, Args&& ... args);
	};

#include "source/io_error_manage.icc"
}