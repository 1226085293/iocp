#pragma once
#include <WS2tcpip.h>
#include "network/websocket/public/io_data.h"
#include "tool/byte.h"
#include "../public/define.h"
#include "event_info.h"
#include "io_context.h"

namespace network::websocket::iocp::server_use {
	// �ͻ���
	class client : public std::enable_shared_from_this<client> {
	private:
		// ��Ҫ������
		struct need_context {
			io_context common;		//����
			io_context recv;		//����
			io_context send;		//����
		};
		event_func& _recv_func;	//���ջص�
		char		_mark;		//������־λ(��ע: ���β��������������������Ӳ���������, ��ֹ���� WSAENOBUFS ����)
		char		_status;	//״̬��Ϣ
	public:
		CRITICAL_SECTION	cri;			//�ٽ���
		SOCKET				socket;			//�׽���
		SOCKADDR_IN			addr_in;		//��ַ��Ϣ
		event_info			info;			//�¼���Ϣ
		need_context		context;		//��Ҫ������
		io_data				io_data;		//io����
		uint16_t			write_wait_ms;	//���͵ȴ�ʱ��(��Ϻϰ�����)
		bool				handshake;		//����״̬

		client(SOCKET sock_, event_func& recv_func_);
		client(const client&) = delete;
		client& operator =(const client&) = delete;
		~client();
		// ���ñ��
		void set_mark(io_mark_type type_);
		// ������
		void del_mark(io_mark_type type_);
		// ��ȡ���״̬
		bool get_mark(io_mark_type type_);
		// ����״̬
		void set_status(client_status type_);
		// ���״̬
		void del_status(client_status type_);
		// ��ȡ�û�״̬
		bool get_status(client_status type_);
	};

#include "source/client.icc"
}