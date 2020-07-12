#pragma once
#include <WS2tcpip.h>
#include "network/websocket/public/io_data.h"
#include "tool/byte.h"
#include "../public/define.h"
#include "event_info.h"
#include "io_context.h"

namespace network::websocket::iocp::server_use {
	// 客户端
	class client : public std::enable_shared_from_this<client> {
	private:
		// 必要上下文
		struct need_context {
			io_context common;		//公共
			io_context recv;		//接收
			io_context send;		//发送
		};
		event_func& _recv_func;	//接收回调
		char		_mark;		//操作标志位(备注: 单次操作限制数据吞吐量增加并发连接数, 防止出现 WSAENOBUFS 错误)
		char		_status;	//状态信息
	public:
		CRITICAL_SECTION	cri;			//临界区
		SOCKET				socket;			//套接字
		SOCKADDR_IN			addr_in;		//地址信息
		event_info			info;			//事件信息
		need_context		context;		//必要上下文
		io_data				io_data;		//io数据
		uint16_t			write_wait_ms;	//发送等待时间(配合合包处理)
		bool				handshake;		//握手状态

		client(SOCKET sock_, event_func& recv_func_);
		client(const client&) = delete;
		client& operator =(const client&) = delete;
		~client();
		// 设置标记
		void set_mark(io_mark_type type_);
		// 清除标记
		void del_mark(io_mark_type type_);
		// 获取标记状态
		bool get_mark(io_mark_type type_);
		// 设置状态
		void set_status(client_status type_);
		// 清除状态
		void del_status(client_status type_);
		// 获取用户状态
		bool get_status(client_status type_);
	};

#include "source/client.icc"
}