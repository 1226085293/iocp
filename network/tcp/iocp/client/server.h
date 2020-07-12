#pragma once
#include <WS2tcpip.h>
#include "network/tcp/public/io_data.h"
#include "tool/byte.h"
#include "../public/define.h"
#include "event_info.h"
#include "io_context.h"

namespace network::tcp::iocp::client_use {
	// �ͻ���
	class server : public std::enable_shared_from_this<server> {
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

		server(SOCKET sock_, event_func& recv_func_);
		server(const server&) = delete;
		server& operator =(const server&) = delete;
		~server();
		// ���ñ��
		void set_mark(io_mark_type type_);
		// ������
		void del_mark(io_mark_type type_);
		// ��ȡ���״̬
		bool get_mark(io_mark_type type_);
		// ����״̬
		void set_status(server_status type_);
		// ���״̬
		void del_status(server_status type_);
		// ��ȡ�û�״̬
		bool get_status(server_status type_);
	};

#include "source/server.icc"
}