#pragma once
#include <cstdint>
#include <chrono>

namespace tool::time {
	// 获取1970年至今的毫秒数
	uint64_t get_ms();
	// 毫秒转化为秒
	double ms_to_s(uint64_t ms_);

#include "source/time.icc"
}