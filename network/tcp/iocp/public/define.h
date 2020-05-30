#pragma once
#include <WinSock2.h>
#include <functional>
#include "../../public/define.h"
#include "../server/event_info.h"

// serverʹ��(��ע: 1.��ֹ�������������ͻ; 2.�˴�ֻ�Ŷ��ⲿ��������)
namespace network::tcp::iocp::server_use {
	// --------------------------------��������
	constexpr uint16_t	socket_load = 512;				//�׽���Ԥ������
	constexpr uint16_t	data_load = 512;				//�û�Ԥ������
	constexpr uint16_t	context_load = data_load * 2;	//������Ԥ������
	constexpr uint16_t	info_load = data_load * 2;		//�¼�Ԥ������
	constexpr uint8_t	entry_count = 64;				//�ص��ṹ�����С
	constexpr uint8_t	shared_num = 5;					//������Դ��
	constexpr uint16_t	write_wait_ms = 50;				//���͵ȴ�ʱ��Ĭ��ֵ
	// --------------------------------ö��
	// �¼�����
	enum class event_type {
		null,
		accept = 0x01,	//����
		recv = 0x02,	//����
		send = 0x04,	//����
		disconnect = 0x08,	//�Ͽ�
	};
	// io����
	enum class io_type {
		null,
		accept = 0x01,	//����
		request_read = 0x02,	//�����
		read = 0x04,	//��
		write = 0x08,	//д
		disconnect = 0x10,	//�Ͽ�
		forced_disconnect = 0x20,	//ǿ�ƶϿ�
	};
	// io��־����
	enum class io_mark_type {
		null,
		read = 0x01,	//��ȡ
		write = 0x02,	//д��
		disconnect = 0x04,	//�Ͽ�
	};
	// user��������
	enum class user_init_type {
		null,
		monitor,	//����
		accept,		//����
		normal,		//����
		disconnect,	//�Ͽ�
	};
	// user״̬
	enum class user_status {
		null,
		wait_close = 0x01,	//�ȴ��ر�
		write_wait = 0x02,	//д��ȴ�
		write_timer = 0x04,	//д�뵹��ʱ
	};
	// --------------------------------��/�ṹ������
	class user;
	class io_context;
	// --------------------------------���ͱ���
	// --------------------------------��������
	using event_func = typename std::function<void(event_info*)>;
	// --------------------------------ָ�����
	using server_ptr = std::shared_ptr<server>;
	using user_ptr = std::shared_ptr<user>;
}