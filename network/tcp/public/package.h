/*	Remarks: 
���Ľṹ: ��ͷ�ṹ + ���� + ��β�ṹ
�洢�ṹ��������Ϣ + ���Ľṹ
*/
#pragma once
#include "network/tcp/public/define.h"

namespace network::tcp {
#pragma pack(push)
#pragma pack(1)
	/*
	������Ϣ
	�洢�ṹ��������Ϣ + ���Ľṹ
	���Ľṹ: ��ͷ�ṹ + ���� + ��β�ṹ
	*/
	struct pack_info {
		slen_t	slen;	//���ͳ���
		slen_t	len;	//����
	};

	/*
	��ͷ�ṹ
	���Ľṹ: ��ͷ�ṹ + ���� + ��β�ṹ
	*/
	struct head_pack {
		char	head;	//��ͷ��ʶ��
		slen_t	len;	//����
	};

	/*
	��β�ṹ
	���Ľṹ: ��ͷ�ṹ + ���� + ��β�ṹ
	*/
	struct tail_pack {
		slen_t	len;	//����
		char	tail;	//��β��ʶ��
	};
#pragma pack(pop)
}