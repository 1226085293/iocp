#include "../io_error_manage.h"

namespace network {
	io_error_manage::io_error_manage() {
		for (int i = 0; i < io_err_tnum; ++i) {
			error_count[i] = error_limit[i] = 0;
		}
	}

	void io_error_manage::add_error(io_err_type type_) {
		++error_count[static_cast<uint32_t>(type_)];
		++continued_error_count[static_cast<uint32_t>(type_)];
		if (error_limit[static_cast<uint32_t>(type_)] && error_count[static_cast<uint32_t>(type_)] >= error_limit[static_cast<uint32_t>(type_)]) {
			error_process[static_cast<uint32_t>(type_)]();
		}
		if (continued_error_limit[static_cast<uint32_t>(type_)] && continued_error_count[static_cast<uint32_t>(type_)] >= continued_error_limit[static_cast<uint32_t>(type_)]) {
			continued_error_process[static_cast<uint32_t>(type_)]();
		}
	}
}