#include "../event_info.h"

namespace network::tcp::iocp::server_use {
	event_info::event_info()
	{
		memset(ip, 0, sizeof(ip));
	}
}