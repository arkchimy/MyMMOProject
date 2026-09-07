#pragma once
#include <cstdint>
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
		int32_t retval = WSAStartup(MAKEWORD(2, 2), &mWsadata);
		RT_ASSERT(retval == 0, "WSAStartup 실패");
	}
	~RAIIwsadata()
	{
		WSACleanup();
	}

	RAIIwsadata(const RAIIwsadata&) = delete;
	RAIIwsadata& operator=(const RAIIwsadata&) = delete;
	RAIIwsadata(RAIIwsadata&&) = delete;
	RAIIwsadata& operator=(RAIIwsadata&&) = delete;

private:
	WSADATA mWsadata;
};