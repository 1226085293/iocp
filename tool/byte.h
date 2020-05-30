#pragma once
#include <cstdint>

namespace tool::byte {
    // 指定位设1
    void setbit(char& value_, uint32_t pos_);
    // 指定位清0
    void clrbit(char& value_, uint32_t pos_);
    // 返回指定位
    char getbit(char& value_, uint32_t pos_);
    // 获取0的包含数量
    uint32_t zero_count(int32_t value_);
    // 获取1的包含数量
    uint32_t one_count(int32_t value_);

#include "source/byte.icc"
}