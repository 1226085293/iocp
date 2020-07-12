#pragma once
#include <functional>
#include "../../public/define.h"

namespace network::websocket {
	// --------------------------------��������
	constexpr uint32_t io_err_tnum = 6;		//io����������
	// --------------------------------ö��
	// ���ݰ���������
	enum class io_err_type {
		invalid,	//��У��(��������ʣ��ռ�/�ռ䲻���������)
		repeat,		//�ظ���(ʱ�����ͬ/��Ϣ������ͬ���Ҳ����ظ���Ϣ�б���)
		request,	//������Ϣ����
		mask,		//�������
		close,		//�����Ͽ�
		opcode,		//opcode���ʹ���
	};
	// --------------------------------��/�ṹ������
	class server;
	class client;
	// --------------------------------���ͱ���
	using slen_t = uint16_t;	//���ͳ��ȵ�λ
	// --------------------------------��������
	// --------------------------------ָ�����
	using server_ptr = std::shared_ptr<server>;
	using client_ptr = std::shared_ptr<client>;

	namespace io_data_use {
		// --------------------------------��������
		constexpr uint16_t	read_buf_size = 8192;	//�������С
		constexpr uint16_t	write_buf_size = 1024;	//д�����С(��Ҫ��������䵥ԪMTU(1360~1500))
		constexpr uint8_t	shared_num = 2;			//������Դ��
		constexpr char		head_mark = '\r';		//��ͷ��ʶ��
		constexpr char		tail_mark = '\n';		//��β��ʶ��
		// --------------------------------ö��
		// websocket��������
		enum class opcode {
			contin,					//����
			text			= 0x1,	//�ı�
			binary			= 0x2,	//������
			unretain_sta	= 0x3,	//�����ǿ���֡��ʼ��
			unretain_end	= 0x7,	//�����ǿ���֡������
			close			= 0x8,	//�Ͽ�
			ping			= 0x9,	//ping
			pong			= 0xa,	//pong
			retain_sta		= 0xb,	//��������֡��ʼ��
			retain_end		= 0xf,	//��������֡������
		};
		// --------------------------------��/�ṹ������
		// --------------------------------���ͱ���
		// --------------------------------��������
		using recv_func = typename std::function<void(std::string&&)>;
		// --------------------------------ָ�����
	}
}