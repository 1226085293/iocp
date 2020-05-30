#pragma once
#include <cstdint>

namespace tool::math {
	// 次方
	namespace power {
		// 获取2n次方
		uint32_t get_power(uint32_t num_);
		// 是否2n次方
		bool is_power(uint32_t num_);
		// 相近2n次方
		uint32_t similar_power(uint32_t num_);
	}

#include "./source/math.icc"
}