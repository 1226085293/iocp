#pragma once
#include "../public/define.h"

namespace network::websocket::iocp::server_use {
	// io������
	struct io_context {
		OVERLAPPED		overlapped;	//�ص�����
		client_ptr		parent;		//����ڵ�
		io_type			type;		//I/O����

		io_context();
		io_context(const io_context&) = delete;
		io_context& operator =(const io_context&) = delete;
		~io_context() = default;

		// �����ص�����
		void reset_overlapped();
	};

#include "source/io_context.icc"
}