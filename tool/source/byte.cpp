#include "../byte.h"

namespace tool::byte {
    uint32_t zero_count(int32_t value_) {
        uint32_t count = 0;
        while (value_ + 1) {
            count++;
            value_ |= (value_ + 1);
        }
        return count;
    }

    uint32_t one_count(int32_t value_) {
        uint32_t count = 0;
        while (value_) {
            count++;
            value_ &= (value_ - 1);
        }
        return count;
    }
}