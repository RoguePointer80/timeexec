#include <stdio.h>
#include "windows.h"
#include "processthreadsapi.h"
#include <string>
#include <string_view>
#include <set>

#define TICKS_PER_SEC 10000000

constexpr ULARGE_INTEGER toULI(FILETIME ft)
{
    ULARGE_INTEGER uli{};
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli;
}

constexpr ULARGE_INTEGER operator-(FILETIME left, FILETIME right)
{
    ULARGE_INTEGER result{};
    result.QuadPart = toULI(left).QuadPart - toULI(right).QuadPart;
    return result;
}

void print_time(std::string_view label, ULARGE_INTEGER duration)
{
    auto micros = static_cast<unsigned int>((duration.QuadPart % TICKS_PER_SEC) / 10);
    auto secs = static_cast<unsigned int>(duration.QuadPart / TICKS_PER_SEC);
    printf("%.*s %6d.%06d sec\r\n", static_cast<int>(label.size()), label.data(), secs, micros);
}

struct InsensitiveCompare {
    bool operator() (const std::wstring& a, const std::wstring& b) const
    {
        int val = CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE, a.c_str(), (int)a.size(), b.c_str(), (int)b.size(), NULL, NULL, 0);
        return val == CSTR_LESS_THAN;
    }
};

bool isCmdInternalCommand(std::wstring cmd)
{
    static std::set<std::wstring, InsensitiveCompare> knownInternalCommands {
        L"ASSOC",
        L"BREAK",
        L"CALL",
        L"CD",
        L"CLS",
        L"COLOR",
        L"COPY",
        L"DATE",
        L"DEL",
        L"DIR",
        L"ECHO",
        L"ERASE",
        L"EXIT",
        L"FOR",
        L"FTYPE",
        L"GOTO",
        L"IF",
        L"MD",
        L"MKDIR",
        L"MKLINK",
        L"MOVE",
        L"PATH",
        L"PAUSE",
        L"POPD",
        L"PROMPT",
        L"PUSHD",
        L"REM",
        L"RENAME",
        L"RD",
        L"RMDIR",
        L"SET",
        L"SETLOCAL",
        L"SHIFT",
        L"START",
        L"TIME",
        L"TITLE",
        L"TYPE",
        L"VER",
        L"VERIFY",
        L"VOL"
    };
    return knownInternalCommands.find(cmd) != knownInternalCommands.end();
}

int wmain(int argc, wchar_t* argv[], wchar_t* envp[])
{
    if (argc <= 1) {
        puts("Missing command-line to timeexec\r\n");
        return 1;
    }

    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};

    std::wstring commandLine;
    if (isCmdInternalCommand(argv[1])) {
        commandLine = L"CMD.EXE /C "; // this is required to support timing built-in commands like 'dir', 'type' or 'echo'
    }
    else {
        commandLine = L"";
    }
    for (int i = 1; i < argc; ++i)
    {
        if(i > 1) commandLine += ' ';
        std::wstring argument{ argv[i] };
        commandLine += argument;
    }
    BOOL create_result = CreateProcessW(NULL ,          // lpApplicationName
                                        &commandLine[0],// lpCommandLine
                                        NULL,           // lpProcessAttributes
                                        NULL,           // lpThreadAttributes
                                        FALSE,          // bInheritHandles
                                        0,              // dwCreationFlags
                                        NULL,           // lpEnvironment
                                        NULL,           // lpCurrentDirectory
                                        &si,            // lpStartupInfo
                                        &pi             // lpProcessInformation
    );
    if (create_result == 0) // If the function succeeds, the return value is nonzero.
    {
        printf("ERROR unable to start process: error code 0x%x \r\n", GetLastError());
        return 2;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);

    FILETIME creation_t, exit_t, kernel_t, user_t;
    GetProcessTimes(pi.hProcess, &creation_t, &exit_t, &kernel_t, &user_t);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    ULARGE_INTEGER realtime;
    realtime = exit_t - creation_t;

    puts("\r\n");
    print_time("real", realtime);
    print_time("user", toULI(user_t));
    print_time("sys ", toULI(kernel_t));

    return exit_code;
}
