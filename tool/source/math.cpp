#include "../math.h"

namespace tool::math {
	namespace power {
		uint32_t similar_power(uint32_t num_) {
			uint32_t count = 0;
			for (; num_; ++count, num_ >>= 1);
			return count;
		}
	}
}