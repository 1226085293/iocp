#pragma once
#include <cstdint>

namespace tool::math {
	// �η�
	namespace power {
		// ��ȡ2n�η�
		uint32_t get_power(uint32_t num_);
		// �Ƿ�2n�η�
		bool is_power(uint32_t num_);
		// ���2n�η�
		uint32_t similar_power(uint32_t num_);
	}

#include "./source/math.icc"
}