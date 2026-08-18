#include <iomanip>
#include <iostream>

#include <windows.h>

#include "Game.h"
#include "../_lib/CrushDump_lib/CrushDump_lib.h"

#pragma comment(lib, "winmm.lib")
constexpr DWORD FRAME_INTERVAL = 20;

CDump dump;

int main()
{

    Game &game = Game::GetInstance();
    HWND hwnd = game.GetWindows();

    game.Start();
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    timeBeginPeriod(1);
    // 고정 타임스텝 20ms (50fps)
    DWORD startTime = timeGetTime();
    DWORD printTime = startTime + 1000;
    DWORD nextTime = startTime + FRAME_INTERVAL;
    bool bRenderSkip = false;
    DWORD frameCnt = 0;
    DWORD renderCnt = 0;
    int delayCnt = 0;
    DWORD updateDelay = 15;

    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            startTime = timeGetTime();
            if (nextTime <= startTime)
            {
                int loopCnt = (startTime - nextTime) / FRAME_INTERVAL  + 1;
                for(int cnt = 0; cnt < loopCnt ; ++cnt)
                {
                    ++frameCnt;
                    game.Update();
                    nextTime += FRAME_INTERVAL;
                }
            }
            // Render Skip
            if (!bRenderSkip)
            {
                game.Render();
                ++renderCnt;
            }
            startTime = timeGetTime();
            if (printTime <= startTime)
            {
                // 밀린 업데이트 체크
                if (frameCnt != 50)
                {
                    delayCnt += 50 - frameCnt;
                }

                //std::cout << std::setw(10) << "Frame : " << std::setw(3) << frameCnt << std::setw(10) << "Render : " << std::setw(3) << renderCnt;
                //std::cout << std::setw(15) << "updateDelay : " << std::setw(3) << updateDelay << std::setw(15) << "DelayCnt : " << std::setw(3) << delayCnt << "\n";
                printTime += 1000;

                frameCnt = 0;
                renderCnt = 0;
            }
            if (nextTime <= startTime)
            {
                bRenderSkip = true;
            }
            else
            {
                bRenderSkip = false;
                Sleep(nextTime - startTime);
            }
        }
    }
    timeEndPeriod(1);
    return 0;
}
