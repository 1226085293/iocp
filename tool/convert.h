#pragma once
#include <string>

// ת��
namespace tool::convert {
	// utf-8 to ���ֽ�
	void utf8_to_multibyte(std::string& str_);
	// ���ֽ� to utf-8
	void multibyte_to_utf8(std::string& str_);
}