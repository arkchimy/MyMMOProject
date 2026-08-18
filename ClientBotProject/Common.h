#pragma once
#include <iostream>
#include <string>
#include <iomanip>
#include <WS2tcpip.h>

#define RT_ASSERT(x,str)\
{\
    if (!(x))\
    {                   \
        std::string s = str;\
        std::cout << __FILE__ << __LINE__ << std::setw(30) << "\n";\
        std::cout << std::setfill('-') << std::setw(30) << s << "\t GetLastError : " << GetLastError() << "\n";\
        __debugbreak(); \
    }\
}


class RAIIwsadata final
{
public:
	RAIIwsadata()
	{
		//성공하면 WSAStartup 함수는 0을 반환
		int retval = WSAStartup(MAKEWORD(2, 2), &wsadata);
		RT_ASSERT(retval == 0, "WSAStartup 실패");
	}
	~RAIIwsadata()
	{
		WSACleanup();
	}
	WSADATA wsadata;
};