#pragma once
#include "network/public/define.h"

namespace network::tcp {
	class server_base {
	private:
	protected:
	public:
		server_status	status;	//״̬

		server_base();
		server_base(const server_base&) = delete;
		server_base& operator =(const server_base&) = delete;
		virtual ~server_base() = 0;
	};

#include "source/server_base.icc"
}