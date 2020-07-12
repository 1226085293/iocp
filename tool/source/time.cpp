#include "../time.h"
#include <sstream>

namespace tool::time {
	std::string get_time_str(char date_symbol, char time_symbol) {
		auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		tm time_info;
		localtime_s(&time_info, &time);
		std::ostringstream output;
		output << time_info.tm_year + 1900 <<
			date_symbol <<
			time_info.tm_mon + 1 <<
			date_symbol <<
			time_info.tm_mday <<
			date_symbol <<
			time_info.tm_hour <<
			time_symbol <<
			time_info.tm_min <<
			time_symbol <<
			time_info.tm_sec;
		return output.str();
	}
}