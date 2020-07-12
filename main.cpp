#include <iostream>
#include <type_traits>
#include <sstream>
#include <tchar.h>
#include "network/websocket/iocp/server.h"
#include "network/tcp/iocp/server.h"
#include "network/tcp/iocp/client.h"
#include "other/debug/dump.h"
#include "tool/time.h"
#include "tool/encode.h"
#include "tool/convert.h"
#include "test/test.h"
#include <variant>
using namespace network;

struct test {
	int* a = new int(123);

	test() {
		printf("构造\n");
	}
	test(const test& that) {
		memcpy_s(this, sizeof(test), &that, sizeof(test));
		printf("拷贝构造\n");
	}
	test(const test&& that) {
		memcpy_s(this, sizeof(test), &that, sizeof(test));
		memset(const_cast<test*>(&that), sizeof(test), 0);
		printf("移动构造\n");
	}
	test& operator =(const test&) {
		printf("同类赋值\n");
	}
	~test() {
		printf("析构\n");
	}
};

int main() {
	debug::init_dump();
	uint64_t start_time;

	auto& timer = time_heap::instance();
	/*auto t1 = timer.add(0.05, 1, [] {
		printf("任务1\n");
		}
	);*/
	//auto t2 = timer.add(5.1, 1, [=] {
	//	//t1->del();
	//	printf("任务2\n");
	//	}
	//);

	//test_timer1();
	//test_timer2();
	tool::encode::sha1(std::string("你"));

	/*debug::dead_lock_check d1;
	test a, b, c;*/

	/*std::thread t1([&] {
		raii::safe_critical r1(d1, &a.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &b.cri);
		printf("线程结束: %ld\n", GetCurrentThreadId());
		}
	);
	std::thread t2([&] {
		raii::safe_critical r1(d1, &b.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &a.cri);
		printf("线程结束: %ld\n", GetCurrentThreadId());
		}
	);/**/

	/*std::thread t1([&] {
		raii::safe_critical r1(d1, &a.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &c.cri);
		printf("线程结束: %ld\n", GetCurrentThreadId());
		}
	);
	std::thread t2([&] {
		raii::safe_critical r1(d1, &b.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &a.cri);
		printf("线程结束: %ld\n", GetCurrentThreadId());
		}
	);
	std::thread t3([&] {
		raii::safe_critical r1(d1, &c.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &b.cri);
		printf("线程结束: %ld\n", GetCurrentThreadId());
		}
	);/**/

	HANDLE eve = CreateEvent(NULL, FALSE, FALSE, NULL);
	while (WaitForSingleObject(eve, INFINITE) != WAIT_OBJECT_0) {}



	/*std::string ip("199.199.199.199");
	auto client = std::make_shared<tcp::iocp::client>();
	client->connect(ip.c_str(), 65535);*/

	//std::atomic<int> test(0);
	//auto server = std::make_shared<tcp::iocp::server>();
	//std::list<tcp::iocp::server_use::event_info*> clients;
	//server->set_event_process(tcp::iocp::server_use::event_type::accept, [&](tcp::iocp::server_use::event_info* info) {
	//	clients.push_back(info);
	//	if (!clients.size()) {
	//		start_time = tool::time::get_ms();
	//	}
	//	else if (clients.size() == 1000) {
	//		printf("消耗时间%I64d\n", (tool::time::get_ms() - start_time));
	//	}
	//	std::ostringstream output;
	//	std::string write_str;
	//	++test;
	//	for (int i = 0; i < 2; ++i) {
	//		output.str("");
	//		output << "8848钛金手机，你值得拥有" << test;
	//		write_str = output.str();
	//		//printf("write_str: %s, 发送时间%I64d\n", write_str.c_str(), tool::time::get_ms());
	//		server->unicast(info->socket, write_str);
	//	}
	//	}
	//);
	//server->set_event_process(tcp::iocp::server_use::event_type::recv, [&](tcp::iocp::server_use::event_info* info) {
	//	//printf("客户端 %s:%d(%I64d) 发送数据: %s\n", info->ip, info->port, info->socket, info->buf);
	//	}
	//);
	//server->set_event_process(tcp::iocp::server_use::event_type::disconnect, [&](tcp::iocp::server_use::event_info* info) {
	//	printf("客户端 %s:%d(%I64d) 断开连接\n", info->ip, info->port, info->socket);
	//	}
	//);
	//server->start(8848);

	struct user_info {
		uint32_t read_count = 0;
	};

	auto websocket_server = std::make_shared<websocket::iocp::server>();
	std::unordered_map<websocket::iocp::server_use::event_info*, user_info> websocket_clients;
	websocket_server->set_event_process(websocket::iocp::server_use::event_type::accept, [&](websocket::iocp::server_use::event_info* info) {
		websocket_clients[info];
		if (!websocket_clients.size()) {
			start_time = tool::time::get_ms();
		}
		else if (websocket_clients.size() == 1000) {
			printf("消耗时间%I64d\n", (tool::time::get_ms() - start_time));
		}
		//std::ostringstream output;
		//std::string write_str;
		//++test;
		//for (int i = 0; i < 2; ++i) {
		//	output.str("");
		//	output << "8848钛金手机，你值得拥有" << test;
		//	write_str = output.str();
		//	//printf("write_str: %s, 发送时间%I64d\n", write_str.c_str(), tool::time::get_ms());
		//	websocket_server->unicast(info->socket, write_str.c_str(), static_cast<websocket::slen_t>(write_str.size()));
		//}
		}
	);

	char* write[] = {(char*)"憨批", (char*)"cnm", (char*)"你再骂！", (char*)"gnmd"};
	websocket_server->set_event_process(websocket::iocp::server_use::event_type::recv, [&](websocket::iocp::server_use::event_info* info) {
		printf("客户端 %s:%d(%I64d) 发送数据: %s\n", info->ip, info->port, info->socket, info->buf);
		std::string write_str(write[websocket_clients[info].read_count++]);
		websocket_server->unicast(info->socket, write_str);
		if (websocket_clients[info].read_count > 3) {
			websocket_clients.erase(info);
			websocket_server->close_client(info);
		}
		}
	);
	websocket_server->set_event_process(websocket::iocp::server_use::event_type::disconnect, [&](websocket::iocp::server_use::event_info* info) {
		printf("客户端 %s:%d(%I64d) 断开连接\n", info->ip, info->port, info->socket);
		}
	);
	websocket_server->start(8848);

	while (WaitForSingleObject(eve, INFINITE) != WAIT_OBJECT_0) {}
}