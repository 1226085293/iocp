#pragma once
#include "network/public/define.h"

namespace network::tcp {
	class client_base {
	private:
	protected:
	public:
		client_status	status;	//·şÎñ¶Ë×´Ì¬

		client_base();
		client_base(const client_base&) = delete;
		client_base& operator =(const client_base&) = delete;
		virtual ~client_base() = 0;
	};

#include "source/client_base.icc"
}