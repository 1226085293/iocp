#include <iostream>
#include "network/tcp/iocp/server.h"
#include "tool/time.h"
#include "test/test.h"

using namespace network::tcp;


template <typename T, int N>
int ArraySize(T(&arr)[N]) { //此处是数组的引用，在C语言中不支持                                                                                                                                                                
	return N;
}

template <int N>
int SOCKSize(SOCKET(&arr)[N]) { //此处是数组的引用，在C语言中不支持                                                                                                                                                                
	return N;
}

int main() {
	test_timer1();
	//test_timer2();
	/*std::cout << SOCKSize(sock) << std::endl;*/
	//test_timer1();
	//test_timer2();
	//test1&& te1e = std::move(get_test());
	//test_timer3();
	HANDLE eve = CreateEvent(NULL, FALSE, FALSE, NULL);
	while (WaitForSingleObject(eve, INFINITE) != WAIT_OBJECT_0) {}
	std::atomic<int> temp(0);
	char ip[16];
	uint64_t start_time;
	std::list<SOCKET> user;

	auto server = std::make_shared<iocp::server>();
	server->close();
	server->set_event_process(iocp::server_use::event_type::accept, [&](iocp::server_use::event_info* info) {
		//printf("客户端 %s:%d(%64d) 连接到服务器\n", inet_ntop(AF_INET, &info->addr_info, ip, sizeof(ip)), ntohs(info->addr_info->sin_port), info->socket);
		if (temp == 0) {
			start_time = tool::time::get_ms();
		}
		else if (temp == 999) {
			printf("消耗时间%I64d\n", (tool::time::get_ms() - start_time));
		}
		char str[64] = {};
		for (int i = 0; i < 1; ++i) {
			memset(str, 0, 64);
			sprintf_s(str, "8848钛金手机，你值得拥有%d", ++temp);
			server->unicast(info->socket, str, 64);
		}
		user.push_back(info->socket);
		SOCKET sock[10];
		server->multicast(sock, "8848钛金手机，你值得拥有", 64);
		}
	);
	server->set_event_process(iocp::server_use::event_type::recv, [&](iocp::server_use::event_info* info) {
		//printf("接收时间%I64d\n", tool::time::get_ms());
		printf("客户端 %s:%d(%I64d) 发送数据: %s\n", info->ip, info->port, info->socket, info->buf);
		/*char str[64] = {};
		for (int i = 0; i < 5; ++i) {
			memset(str, 0, 64);
			sprintf_s(str, "8848钛金手机，你值得拥有%d", ++temp);
			server->send_to_individual(info->socket, str, 64);
		}*/
		}
	);
	server->set_event_process(iocp::server_use::event_type::disconnect, [&](iocp::server_use::event_info* info) {
		//printf("接收时间%I64d\n", tool::time::get_ms());
		//const SOCKADDR_IN* addr_in = info->addr_info;
		memset(ip, 0, sizeof(ip));
		//printf("客户端 %s:%d(%I64d) 断开连接\n", inet_ntop(AF_INET, &addr_in->sin_addr, ip, sizeof(ip)), ntohs(addr_in->sin_port), info->socket);
		}
	);
	server->start(8848);
	server->close();


	while (WaitForSingleObject(eve, INFINITE) != WAIT_OBJECT_0) {}
}