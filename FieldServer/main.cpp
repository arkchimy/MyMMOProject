#include "FieldServer.h"
#include "../_lib/CrushDump_lib/CrushDump_lib.h"
#pragma comment(lib,"winmm.lib")
#include <timeapi.h>
#include "../_lib/MTProfiler_Lib/MTProfiler_Lib.h"

#include <consoleapi2.h>
#include <conio.h>

int main()
{
	SetConsoleOutputCP(65001);
	
	timeBeginPeriod(1);
	CDump dump;
	
	contents::FieldServer* server = new contents::FieldServer();
	// TODO : 서버 종료 대기
	server->Start();

	DWORD startTime = timeGetTime();
	DWORD nextTime = startTime + 1000;

	int32_t deleteCnt = 0;
	while (1)
	{
		startTime = timeGetTime();
		char ch = _getch();
		if (ch == 'a' || ch == 'A')
		{
			CProfileRegistry::GetInstance().CreateProfile(L"FieldSever");
		}
		else if (ch == 'd' || ch == 'D')
		{
			CProfileRegistry::GetInstance().ResetEntry();
		}
		else if (ch == VK_ESCAPE)
		{
			delete server;
			break;
		}
		if (startTime < nextTime)
		{
			Sleep(nextTime - startTime);
		}
	}
	std::cout << "종료 절차 완료.\n";
	system("pause");

	timeEndPeriod(1);
	return 0;
}