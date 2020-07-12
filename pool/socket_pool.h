#pragma once
#include <deque>
#include <WinSock2.h>
#include "other/raii/critical.h"
#include "tool/byte.h"

enum class socket_option {
	null,
	non_block = 0x01,		//非阻塞
	port_multiplex = 0x02,	//端口复用
};

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
	SOCKET get(char option = 0);
	// 回收socket
	void rec(SOCKET sock);
	// 删除socket
	void del(SOCKET& sock, bool reset = false);
};

#include "source/socket_pool.tcc"