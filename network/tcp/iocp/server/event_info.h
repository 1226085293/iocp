#pragma once
#include <cstdint>
#include <WinSock2.h>

namespace network::tcp::iocp::server_use {
	// �¼���Ϣ
	class event_info {
	private:
		//user_ptr&		_user;	//���
	public:
		SOCKET			socket;	//�׽���
		char			ip[16];	//��ַ
		uint16_t		port;	//�˿�
		char*			buf;	//�¼�����

		event_info();
		event_info(const event_info&) = delete;
		event_info& operator =(const event_info&) = delete;
		~event_info() = default;
	};
}