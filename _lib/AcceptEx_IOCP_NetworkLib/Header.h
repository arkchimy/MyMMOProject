#pragma once

#include <cstdint>

#pragma pack(1)
struct Header
{
    int16_t Len;
    int8_t RandKey;
};
#pragma pack()