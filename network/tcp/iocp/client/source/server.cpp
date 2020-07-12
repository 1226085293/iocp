#include <atomic>
#include "../server.h"

namespace network::tcp::iocp::client_use {
	static std::atomic<int> test_count = 0;

	server::server(SOCKET sock_, event_func& recv_func_) :
		_recv_func(recv_func_),
		_mark(0),
		_status(0),
		socket(sock_),
		io_data([&](std::string&& str)
			{
				info.buf = &str.front();
				_recv_func(&info);
			}
		),
		write_wait_ms(client_use::write_wait_ms)
	{
		InitializeCriticalSection(&cri);
		memset(&addr_in, 0, sizeof(SOCKADDR_IN));
		context.common.type = io_type::accept;
		context.send.type = io_type::write;
	}

	server::~server() {
		DeleteCriticalSection(&cri);
		printf("É¾³ýserverÊýÁ¿: %d\n", ++test_count);
	}
}