#pragma once
#include "../public/define.h"

namespace network::websocket::iocp::server_use {
	// io上下文
	struct io_context {
		OVERLAPPED		overlapped;	//重叠数据
		client_ptr		parent;		//管理节点
		io_type			type;		//I/O类型

		io_context();
		io_context(const io_context&) = delete;
		io_context& operator =(const io_context&) = delete;
		~io_context() = default;

		// 重置重叠数据
		void reset_overlapped();
	};

#include "source/io_context.icc"
}