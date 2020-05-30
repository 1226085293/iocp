#pragma once
#include <WS2tcpip.h>
#include "network/tcp/public/io_data.h"
#include "tool/byte.h"
#include "../public/define.h"

namespace network::tcp::iocp::server_use {
	// 客户端信息
	class user {
	private:
		// 必要上下文
		struct need_context {
			io_context* accept;		//连接
			io_context* send;		//发送
			io_context* recv;		//接收
			io_context* disconnect;	//断开
		};

		event_func& _recv_func;	//接收回调
		char		_mark;		//操作标志位(备注: 单次操作限制数据吞吐量增加并发连接数, 防止出现 WSAENOBUFS 错误)
		char		_status;	//状态信息
		SOCKADDR_IN	_addr_in;	//地址信息
	public:
		CRITICAL_SECTION	cri;			//临界区
		SOCKET				socket;			//套接字
		event_info			info;			//事件信息
		need_context		context;		//必要上下文
		io_data				io_data;		//io数据
		uint16_t			write_wait_ms;	//发送等待时间(配合合包处理)

		user(event_func& recv_func_);
		user(const user&) = delete;
		user& operator =(const user&) = delete;
		~user();
		// 更新ip/port信息
		void update_addr_in(SOCKADDR_IN* addr_);
		// 设置标记
		void set_mark(io_mark_type type_);
		// 清除标记
		void del_mark(io_mark_type type_);
		// 获取标记状态
		bool get_mark(io_mark_type type_);
		// 设置状态
		void set_status(user_status type_);
		// 清除状态
		void del_status(user_status type_);
		// 获取用户状态
		bool get_status(user_status type_);
	};

#include "source/user.icc"
}