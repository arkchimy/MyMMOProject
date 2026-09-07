#include "pch.h"
#include "Common.h"

MyHeap gHeap;

void *operator new(size_t size, const char *file, int32_t line)
{
    void *ptr = HeapAlloc(gHeap.mSMyHeap, HEAP_GENERATE_EXCEPTIONS, size);
    RT_ASSERT(ptr != nullptr);

    return ptr;
}

void *operator new[](size_t size, const char *file, int32_t line)
{
    void *ptr = HeapAlloc(gHeap.mSMyHeap, HEAP_GENERATE_EXCEPTIONS, size);
    return ptr;
}

void operator delete(void *ptr, const char *file, int32_t line)
{
}

void operator delete[](void *ptr, const char *file, int32_t line)
{
}



MyHeap::MyHeap()
    : mSMyHeap(INVALID_HANDLE_VALUE)
{
    ULONG info = 2;
    mSMyHeap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
    RT_ASSERT(mSMyHeap != nullptr);
    HeapSetInformation(mSMyHeap, HeapCompatibilityInformation, &info, sizeof(info));
}


MyHeap::~MyHeap()
{
    RT_ASSERT(mSMyHeap != INVALID_HANDLE_VALUE);
    RT_ASSERT(mSMyHeap != nullptr);
    HeapDestroy(mSMyHeap);
}
