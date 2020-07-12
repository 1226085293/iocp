/*	Remarks: 
���Ľṹ: ��ͷ�ṹ + ���� + ��β�ṹ
�洢�ṹ��������Ϣ + ���Ľṹ
*/
#pragma once
#include "network/websocket/public/define.h"

namespace network::websocket {
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
	����ṹ
	*/
	class request {
	private:
		bool update_child(std::string& info_, std::string& data_, size_t& result_index_, size_t& find_index_, const char* mark);
	protected:
	public:
		bool		init;						//��ʼ��״̬
		std::string user_agent;					//�û�������Ϣ
		std::string origin;						//��Դ
		std::string accept_encoding;			//֧�ֵ�ѹ����ʽ
		std::string sec_websocket_key;			//websocket��Կ

		request();
		request(const request&) = delete;
		request& operator =(const request&) = delete;
		~request() = default;

		// ������Ϣ
		bool update(std::string& info_);
	};

	/*
	Ӧ��ṹ
	*/
	struct handshake {
		std::string protocol_info;			//Э����Ϣ
		std::string connection;				//��������
		std::string upgrade;				//����Э��
		std::string content_encoding;		//ѡ���ѹ����ʽ
		std::string content_language;		//ѡ��Ŀͻ�������
		std::string sec_websocket_accept;	//websocket��Կ(base64(sha1(sec_websocket_key + 258EAFA5-E914-47DA-95CA-C5AB0DC85B11)))

		handshake(std::string key_);

		// ���ñ���Э��
		bool set_encode(request& request_, const char* type_);
		// �ϲ�
		std::string merge();
	};
#pragma pack(pop)

#include "source/package.icc"
}