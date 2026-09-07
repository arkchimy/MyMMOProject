#pragma once
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif


#ifndef MY_ASSERT
#define MY_ASSERT(x,str)\
    do{\
    if (!(x))\
    {                   \
        std::string s = str;\
        std::cout << __FILE__ << __LINE__ << std::setw(30) << "\n";\
        std::cout << std::setfill('-') << std::setw(30) << s << "\t GetLastError : " << GetLastError() << "\n";\
        __debugbreak(); \
    }\
	}while (0)
#endif

#ifndef RT_ASSERT
#define RT_ASSERT(x) \
    if (!(x))        \
        __debugbreak();
#endif

#include <Windows.h>

void *operator new(size_t size,const char* file, int32_t line);
void *operator new[](size_t size, const char *file, int32_t line);
void operator delete(void *ptr, const char *file, int32_t line);
void operator delete[](void *ptr, const char *file, int32_t line);

struct MyHeap
{
    friend void *operator new(size_t size, const char *file, int32_t line);
    friend void *operator new[](size_t size, const char *file, int32_t line);
    friend struct MyDeleteHelper;

    MyHeap();
    ~MyHeap();

    MyHeap(const MyHeap &) = delete;
    MyHeap &operator=(const MyHeap &) = delete;
    MyHeap(MyHeap &&) = delete;
    MyHeap &operator=(MyHeap &&) = delete;

  private:
    HANDLE mSMyHeap;
};

extern MyHeap gHeap;
struct MyDeleteHelper
{
    template<typename  T>
    void operator,(T* ptr)
    {
        ptr->~T();
        BOOL bSuccess = HeapFree(gHeap.mSMyHeap, 0, ptr);
        RT_ASSERT(bSuccess != 0);
    }
};
#define MY_NEW new (__FILE__,__LINE__)
#define MY_DELETE MyDeleteHelper {}, 