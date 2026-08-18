#include "Common.h"
#include <cstring>
#include <iostream>
#include <iomanip>

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>

void MyAssert(bool x, const char* str)
{
    if (!(x))
    {                   
        std::string s = str;
        std::cout << std::setfill('-') << std::setw(30) << s << "\t GetLastError : " << GetLastError() << "\n";
        __debugbreak(); 
    }

}
