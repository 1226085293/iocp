#include "../io_context.h"

namespace network::tcp::iocp::server_use {
	io_context::io_context(user_ptr& parent_, fixed_memory_pool<io_context, context_load>* rec_pool_) :
		_rec_pool(rec_pool_),
		parent(parent_),
		type(io_type::null)
	{
		memset(&overlapped, 0, sizeof(OVERLAPPED));
	}

	io_context::~io_context() {
		parent.reset();
		_rec_pool->deallocate(this);
	}
}