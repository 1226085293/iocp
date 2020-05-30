#pragma once
#include <cstdint>

namespace tool::byte {
    // ָ��λ��1
    void setbit(char& value_, uint32_t pos_);
    // ָ��λ��0
    void clrbit(char& value_, uint32_t pos_);
    // ����ָ��λ
    char getbit(char& value_, uint32_t pos_);
    // ��ȡ0�İ�������
    uint32_t zero_count(int32_t value_);
    // ��ȡ1�İ�������
    uint32_t one_count(int32_t value_);

#include "source/byte.icc"
}