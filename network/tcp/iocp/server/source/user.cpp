#include <atomic>
#include "../user.h"

namespace network::tcp::iocp::server_use {
	static std::atomic<int> test_count = 0;

	user::user(event_func& recv_func_) :
		_recv_func(recv_func_),
		_mark(0),
		_status(0),
		socket(INVALID_SOCKET),
		io_data([&](std::string&& str) {
		info.buf = &str.front();
		_recv_func(&info);
			}
		),
		write_wait_ms(server_use::write_wait_ms)
	{
		InitializeCriticalSection(&cri);
		memset(&_addr_in, 0, sizeof(SOCKADDR_IN));
		memset(&context, 0, sizeof(need_context));
	}

	user::~user() {
		DeleteCriticalSection(&cri);
		context.send = context.recv = nullptr;
		printf("É¾³ýuserÊýÁ¿: %d\n", ++test_count);
	}
}