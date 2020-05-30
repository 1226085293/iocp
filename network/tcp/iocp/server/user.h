#pragma once
#include <WS2tcpip.h>
#include "network/tcp/public/io_data.h"
#include "tool/byte.h"
#include "../public/define.h"

namespace network::tcp::iocp::server_use {
	// �ͻ�����Ϣ
	class user {
	private:
		// ��Ҫ������
		struct need_context {
			io_context* accept;		//����
			io_context* send;		//����
			io_context* recv;		//����
			io_context* disconnect;	//�Ͽ�
		};

		event_func& _recv_func;	//���ջص�
		char		_mark;		//������־λ(��ע: ���β��������������������Ӳ���������, ��ֹ���� WSAENOBUFS ����)
		char		_status;	//״̬��Ϣ
		SOCKADDR_IN	_addr_in;	//��ַ��Ϣ
	public:
		CRITICAL_SECTION	cri;			//�ٽ���
		SOCKET				socket;			//�׽���
		event_info			info;			//�¼���Ϣ
		need_context		context;		//��Ҫ������
		io_data				io_data;		//io����
		uint16_t			write_wait_ms;	//���͵ȴ�ʱ��(��Ϻϰ�����)

		user(event_func& recv_func_);
		user(const user&) = delete;
		user& operator =(const user&) = delete;
		~user();
		// ����ip/port��Ϣ
		void update_addr_in(SOCKADDR_IN* addr_);
		// ���ñ��
		void set_mark(io_mark_type type_);
		// ������
		void del_mark(io_mark_type type_);
		// ��ȡ���״̬
		bool get_mark(io_mark_type type_);
		// ����״̬
		void set_status(user_status type_);
		// ���״̬
		void del_status(user_status type_);
		// ��ȡ�û�״̬
		bool get_status(user_status type_);
	};

#include "source/user.icc"
}