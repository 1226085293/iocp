#pragma once
#include <functional>
#include "../../public/define.h"

namespace network::tcp {
	// --------------------------------��������
	// --------------------------------ö��
	// --------------------------------��/�ṹ������
	class server;
	class client;
	// --------------------------------���ͱ���
	using slen_t = uint16_t;	//���ͳ��ȵ�λ
	// --------------------------------��������
	// --------------------------------ָ�����

	namespace io_data_use {
		// --------------------------------��������
		constexpr uint16_t	read_buf_size = 8192;	//�������С
		constexpr uint16_t	write_buf_size = 1024;	//д�����С(��Ҫ��������䵥ԪMTU(1360~1500))
		constexpr uint8_t	shared_num = 2;			//������Դ��
		constexpr char		head_mark = '\r';		//��ͷ��ʶ��
		constexpr char		tail_mark = '\n';		//��β��ʶ��
		// --------------------------------ö��
		// --------------------------------��/�ṹ������
		// --------------------------------���ͱ���
		// --------------------------------��������
		using recv_func = typename std::function<void(std::string&&)>;
		// --------------------------------ָ�����
	}
}