#pragma once
#include "../public/define.h"
#include "pool/fixed_memory_pool.h"

namespace network::tcp::iocp::server_use {
	// io������
	class io_context {
	private:
		fixed_memory_pool<io_context, context_load>* _rec_pool;		//���ճ�
	public:
		OVERLAPPED	overlapped;	//I/O��Ϣ
		user_ptr	parent;		//����ڵ�
		io_type		type;		//I/O����

		io_context(user_ptr& parent_, fixed_memory_pool<io_context, context_load>* rec_pool_);
		io_context(const io_context&) = delete;
		io_context& operator =(const io_context&) = delete;
		~io_context();
	};
}