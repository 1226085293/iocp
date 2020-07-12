#pragma once
#include <cstdint>
#include <WinSock2.h>
#include "../public/define.h"

namespace network::websocket::iocp::server_use {
	// �¼���Ϣ
	struct event_info {
		SOCKET			socket;	//�׽���
		char			ip[16];	//��ַ
		uint16_t		port;	//�˿�
		char*			buf;	//�¼�����

		event_info();
		event_info(const event_info&) = delete;
		event_info& operator =(const event_info&) = delete;
		~event_info() = default;
	};

#include "source/event_info.icc"
}