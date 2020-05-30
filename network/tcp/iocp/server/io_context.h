#pragma once
#include "../public/define.h"
#include "pool/fixed_memory_pool.h"

namespace network::tcp::iocp::server_use {
	// io上下文
	class io_context {
	private:
		fixed_memory_pool<io_context, context_load>* _rec_pool;		//回收池
	public:
		OVERLAPPED	overlapped;	//I/O信息
		user_ptr	parent;		//管理节点
		io_type		type;		//I/O类型

		io_context(user_ptr& parent_, fixed_memory_pool<io_context, context_load>* rec_pool_);
		io_context(const io_context&) = delete;
		io_context& operator =(const io_context&) = delete;
		~io_context();
	};
}