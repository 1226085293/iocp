#pragma once
#include <cstdint>
#include <WinSock2.h>

namespace network::tcp::iocp::server_use {
	// 事件信息
	class event_info {
	private:
		//user_ptr&		_user;	//玩家
	public:
		SOCKET			socket;	//套接字
		char			ip[16];	//地址
		uint16_t		port;	//端口
		char*			buf;	//事件数据

		event_info();
		event_info(const event_info&) = delete;
		event_info& operator =(const event_info&) = delete;
		~event_info() = default;
	};
}