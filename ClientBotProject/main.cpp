// ClientBotProject.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "ClientBot.h"
#include "../_lib/CrushDump_lib/CrushDump_lib.h"
#pragma comment(lib,"Winmm.lib")
#include <Windows.h>
#include <timeapi.h>
#include "../_lib/MTProfiler_Lib/MTProfiler_Lib.h"

CDump dump;
int main()
{
	timeBeginPeriod(1);
	{
		ClientBot bot;
	}

	std::cout << "모든 필드스레드 종료 완료\n";
	system("pause");
	timeEndPeriod(1);
}

