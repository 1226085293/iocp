#include <atomic>
#include "../client.h"

namespace network::websocket::iocp::server_use {
	static std::atomic<int> test_count = 0;

	client::client(SOCKET sock_, event_func& recv_func_) :
		_recv_func(recv_func_),
		_mark(0),
		_status(0),
		socket(sock_),
		io_data([&](std::string&& str_)
			{
				info.buf = &str_.front();
				_recv_func(&info);
			}
		),
		write_wait_ms(server_use::write_wait_ms),
		handshake(false)
	{
		InitializeCriticalSection(&cri);
		memset(&addr_in, 0, sizeof(SOCKADDR_IN));
		context.common.type = io_type::accept;
		context.send.type = io_type::write;
	}

	client::~client() {
		DeleteCriticalSection(&cri);
		printf("É¾³ýclientÊýÁ¿: %d\n", ++test_count);
	}
}