#pragma once

void MyAssert(bool x, const char* str);

#ifndef RT_ASSERT
#define RT_ASSERT(x) \
    if (!(x))        \
        __debugbreak();
#endif
