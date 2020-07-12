#pragma once
#include <string>

// 转换
namespace tool::convert {
	// utf-8 to 多字节
	void utf8_to_multibyte(std::string& str_);
	// 多字节 to utf-8
	void multibyte_to_utf8(std::string& str_);
}