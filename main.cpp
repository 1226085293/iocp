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
		printf("����\n");
	}
	test(const test& that) {
		memcpy_s(this, sizeof(test), &that, sizeof(test));
		printf("��������\n");
	}
	test(const test&& that) {
		memcpy_s(this, sizeof(test), &that, sizeof(test));
		memset(const_cast<test*>(&that), sizeof(test), 0);
		printf("�ƶ�����\n");
	}
	test& operator =(const test&) {
		printf("ͬ�ำֵ\n");
	}
	~test() {
		printf("����\n");
	}
};

int main() {
	debug::init_dump();
	uint64_t start_time;

	auto& timer = time_heap::instance();
	/*auto t1 = timer.add(0.05, 1, [] {
		printf("����1\n");
		}
	);*/
	//auto t2 = timer.add(5.1, 1, [=] {
	//	//t1->del();
	//	printf("����2\n");
	//	}
	//);

	//test_timer1();
	//test_timer2();
	tool::encode::sha1(std::string("��"));

	/*debug::dead_lock_check d1;
	test a, b, c;*/

	/*std::thread t1([&] {
		raii::safe_critical r1(d1, &a.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &b.cri);
		printf("�߳̽���: %ld\n", GetCurrentThreadId());
		}
	);
	std::thread t2([&] {
		raii::safe_critical r1(d1, &b.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &a.cri);
		printf("�߳̽���: %ld\n", GetCurrentThreadId());
		}
	);/**/

	/*std::thread t1([&] {
		raii::safe_critical r1(d1, &a.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &c.cri);
		printf("�߳̽���: %ld\n", GetCurrentThreadId());
		}
	);
	std::thread t2([&] {
		raii::safe_critical r1(d1, &b.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &a.cri);
		printf("�߳̽���: %ld\n", GetCurrentThreadId());
		}
	);
	std::thread t3([&] {
		raii::safe_critical r1(d1, &c.cri);
		Sleep(50);
		raii::safe_critical r2(d1, &b.cri);
		printf("�߳̽���: %ld\n", GetCurrentThreadId());
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
	//		printf("����ʱ��%I64d\n", (tool::time::get_ms() - start_time));
	//	}
	//	std::ostringstream output;
	//	std::string write_str;
	//	++test;
	//	for (int i = 0; i < 2; ++i) {
	//		output.str("");
	//		output << "8848�ѽ��ֻ�����ֵ��ӵ��" << test;
	//		write_str = output.str();
	//		//printf("write_str: %s, ����ʱ��%I64d\n", write_str.c_str(), tool::time::get_ms());
	//		server->unicast(info->socket, write_str);
	//	}
	//	}
	//);
	//server->set_event_process(tcp::iocp::server_use::event_type::recv, [&](tcp::iocp::server_use::event_info* info) {
	//	//printf("�ͻ��� %s:%d(%I64d) ��������: %s\n", info->ip, info->port, info->socket, info->buf);
	//	}
	//);
	//server->set_event_process(tcp::iocp::server_use::event_type::disconnect, [&](tcp::iocp::server_use::event_info* info) {
	//	printf("�ͻ��� %s:%d(%I64d) �Ͽ�����\n", info->ip, info->port, info->socket);
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
			printf("����ʱ��%I64d\n", (tool::time::get_ms() - start_time));
		}
		//std::ostringstream output;
		//std::string write_str;
		//++test;
		//for (int i = 0; i < 2; ++i) {
		//	output.str("");
		//	output << "8848�ѽ��ֻ�����ֵ��ӵ��" << test;
		//	write_str = output.str();
		//	//printf("write_str: %s, ����ʱ��%I64d\n", write_str.c_str(), tool::time::get_ms());
		//	websocket_server->unicast(info->socket, write_str.c_str(), static_cast<websocket::slen_t>(write_str.size()));
		//}
		}
	);

	char* write[] = {(char*)"����", (char*)"cnm", (char*)"�����", (char*)"gnmd"};
	websocket_server->set_event_process(websocket::iocp::server_use::event_type::recv, [&](websocket::iocp::server_use::event_info* info) {
		printf("�ͻ��� %s:%d(%I64d) ��������: %s\n", info->ip, info->port, info->socket, info->buf);
		std::string write_str(write[websocket_clients[info].read_count++]);
		websocket_server->unicast(info->socket, write_str);
		if (websocket_clients[info].read_count > 3) {
			websocket_clients.erase(info);
			websocket_server->close_client(info);
		}
		}
	);
	websocket_server->set_event_process(websocket::iocp::server_use::event_type::disconnect, [&](websocket::iocp::server_use::event_info* info) {
		printf("�ͻ��� %s:%d(%I64d) �Ͽ�����\n", info->ip, info->port, info->socket);
		}
	);
	websocket_server->start(8848);

	while (WaitForSingleObject(eve, INFINITE) != WAIT_OBJECT_0) {}
}