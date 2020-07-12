#pragma once
#include <cstdint>
#include <chrono>
#include <string>

namespace tool::time {
	// 获取1970年至今的毫秒数
	uint64_t get_ms();
	// 毫秒转化为秒
	double ms_to_s(uint64_t ms_);
	// 获取当前时间(年月日时分秒)字符串
	std::string get_time_str(char date_symbol, char time_symbol);

#include "source/time.icc"
}