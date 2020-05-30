#pragma once
#include <deque>
#include <WinSock2.h>
#include "raii/critical.h"

template <uint32_t load_num = 512>
class socket_pool {
private:
	CRITICAL_SECTION	_cri;			//临界区
	int					_protocol;		//网络类型
	std::deque<SOCKET>	_use_socket;	//可使用的socket

	// 添加新的socket
	void add_new();
protected:
public:
	socket_pool(int protocol);
	socket_pool(const socket_pool&) = delete;
	socket_pool& operator =(const socket_pool&) = delete;
	~socket_pool();

	// 获取socket
	SOCKET get();
	// 回收socket
	void rec(SOCKET sock);
	// 删除socket
	void del(SOCKET& sock, bool reset = true);
};

#include "source/socket_pool.tcc"