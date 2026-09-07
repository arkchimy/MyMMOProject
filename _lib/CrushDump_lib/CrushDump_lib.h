#pragma once
#pragma comment(lib, "DbgHelp.lib")

#include <cstdint>

#include <Windows.h>
#include <iostream>
#include <psapi.h>

#include <strsafe.h>
#include <string>

#include <minidumpapiset.h>

class CDump
{
  public:
    CDump()
    {
        mDumpCount = 0;
        _invalid_parameter_handler oldHandler = nullptr, newHandler = nullptr; //
        newHandler = myInvalidParameterHandler;
        oldHandler = _set_invalid_parameter_handler(newHandler);

        _CrtSetReportMode(_CRT_WARN, 0);
        _CrtSetReportMode(_CRT_ASSERT, 0);
        _CrtSetReportMode(_CRT_ERROR, 0);

        _CrtSetReportHook(customReportHook);

        _set_purecall_handler(myPurecallHandler);
        setHandlerDump();
    }

    CDump(const CDump &) = delete;
    CDump &operator=(const CDump &) = delete;
    CDump(CDump &&) = delete;
    CDump &operator=(CDump &&) = delete;

  private:
    static LONG WINAPI myExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
    {
        int32_t iWorkingMemory = 0;
        SYSTEMTIME stNowTime{};

        long DumpCount = _InterlockedIncrement(&mDumpCount);

        HANDLE hProcess = 0;
        PROCESS_MEMORY_COUNTERS pmc{};

        hProcess = GetCurrentProcess();

        if (NULL == hProcess)
            return 0;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
        {
            iWorkingMemory = (int32_t)(pmc.WorkingSetSize / 1024 / 1024);
        }
        CloseHandle(hProcess);

        GetLocalTime(&stNowTime);
        std::wstring filename = L"Dump_";
        filename += std::to_wstring(stNowTime.wYear);
        filename += L"_";
        filename += std::to_wstring(stNowTime.wMonth);
        filename += L"_";
        filename += std::to_wstring(stNowTime.wDay);
        filename += L"_Time_";
        filename += std::to_wstring(stNowTime.wHour);
        filename += L"_";
        filename += std::to_wstring(stNowTime.wMinute);
        filename += L"_";
        filename += std::to_wstring(DumpCount);
        filename += L".dmp";
        wprintf(L"\n\n\n!!! Crash Error !!!   %d.%d.%d/%d:%d:%d \n",
                stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);

        wprintf(L"Now Save Dump file... \n");

        HANDLE hDumpFile = ::CreateFile(filename.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hDumpFile != INVALID_HANDLE_VALUE)
        {
            _MINIDUMP_EXCEPTION_INFORMATION MinidumpExceptioninformation;

            MinidumpExceptioninformation.ThreadId = ::GetCurrentThreadId();
            MinidumpExceptioninformation.ExceptionPointers = pExceptionPointer;
            MinidumpExceptioninformation.ClientPointers = TRUE;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithFullMemory, &MinidumpExceptioninformation, NULL, NULL);
            CloseHandle(hDumpFile);

            wprintf(L"CrashDump Save Finish !");
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }
    static void setHandlerDump()
    {
        SetUnhandledExceptionFilter(myExceptionFilter);
    }
    static void myInvalidParameterHandler(const wchar_t *expression, const wchar_t *function, const wchar_t *file, unsigned int line, uintptr_t pReserved) noexcept
    {
        crash();
    }
    static void crash(void) noexcept
    {
        __debugbreak();
    }
    static int customReportHook(int ireposttype, char *message, int *returnvalue) noexcept
    {
        crash();
        return true;
    }
    static void myPurecallHandler(void) noexcept
    {
        crash();
    }

  private:
    inline static long mDumpCount;
};
