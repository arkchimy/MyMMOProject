#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define RT_ASSERT(x)        \
    do                      \
    {                       \
        if (!(x))           \
        {                   \
            __debugbreak(); \
        };                  \
    } while (0)
#define MY_NEW new
#define MY_DELETE delete
struct Vec4
{
    float v[4];
};

struct Vec2
{
    float v[2];
};

struct Vertex
{
    Vec4 pos;
    Vec2 uv;
};

struct CBData
{
    float matrix[4][4];
    float uvOffset[2];
    float uvScale[2];
};