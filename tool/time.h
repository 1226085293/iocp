#pragma once
#include <cstdint>
#include <chrono>

namespace tool::time {
	// ��ȡ1970������ĺ�����
	uint64_t get_ms();
	// ����ת��Ϊ��
	double ms_to_s(uint64_t ms_);

#include "source/time.icc"
}