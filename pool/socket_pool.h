#pragma once
#include <deque>
#include <WinSock2.h>
#include "raii/critical.h"

template <uint32_t load_num = 512>
class socket_pool {
private:
	CRITICAL_SECTION	_cri;			//�ٽ���
	int					_protocol;		//��������
	std::deque<SOCKET>	_use_socket;	//��ʹ�õ�socket

	// ����µ�socket
	void add_new();
protected:
public:
	socket_pool(int protocol);
	socket_pool(const socket_pool&) = delete;
	socket_pool& operator =(const socket_pool&) = delete;
	~socket_pool();

	// ��ȡsocket
	SOCKET get();
	// ����socket
	void rec(SOCKET sock);
	// ɾ��socket
	void del(SOCKET& sock, bool reset = true);
};

#include "source/socket_pool.tcc"