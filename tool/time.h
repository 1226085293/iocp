#pragma once
#include <cstdint>
#include <chrono>
#include <string>

namespace tool::time {
	// ��ȡ1970������ĺ�����
	uint64_t get_ms();
	// ����ת��Ϊ��
	double ms_to_s(uint64_t ms_);
	// ��ȡ��ǰʱ��(������ʱ����)�ַ���
	std::string get_time_str(char date_symbol, char time_symbol);

#include "source/time.icc"
}