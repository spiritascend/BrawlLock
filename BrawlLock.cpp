#include <windows.h>
#include <tlhelp32.h>
#include <iostream>


static DWORD GetProcessIdByName(const std::wstring& procName) {
    PROCESSENTRY32W pe{ sizeof(pe) };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    for (bool ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, procName.c_str()) == 0) {
            CloseHandle(snap);
            return pe.th32ProcessID;
        }
    }
    CloseHandle(snap);
    return 0;
}

struct HandleData { DWORD pid; HWND hwnd; };
static BOOL CALLBACK EnumWindowsProc(HWND h, LPARAM l) {
    auto data = reinterpret_cast<HandleData*>(l);
    DWORD winPid = 0;
    GetWindowThreadProcessId(h, &winPid);

    if (winPid != data->pid || !IsWindowVisible(h) || GetWindowTextLengthW(h) == 0) {
        return TRUE;
    }

    data->hwnd = h;
    return FALSE;
}

static HWND GetMainWindow(DWORD pid) {
    HandleData data{ pid, nullptr };
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.hwnd;
}


RECT GetClientScreenRect(HWND hwnd) {
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    POINT ul{ rcClient.left,  rcClient.top };
    POINT lr{ rcClient.right, rcClient.bottom };
    ClientToScreen(hwnd, &ul);
    ClientToScreen(hwnd, &lr);
    RECT rcScreen{ ul.x, ul.y, lr.x, lr.y };
    return rcScreen;
}

int main() {
    const std::wstring procName = L"Brawlhalla.exe";

    while (true) {
        DWORD pid = 0;
        printf("Waiting for process '%ls'...\n", procName.c_str());
        while (!(pid = GetProcessIdByName(procName))) {
            Sleep(1000);
        }
        printf("Process '%ls' found (PID=%u)\n", procName.c_str(), pid);

        HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) {
            printf("Failed to open process handle: %u\n", GetLastError());
            continue;
        }

        HWND hwnd = GetMainWindow(pid);
        if (!hwnd) {
            printf("Found PID=%u but no visible window.\n", pid);
            CloseHandle(hProc);
            continue;
        }

        printf("Found window handle: 0x%p\n", (void*)hwnd);

        bool isClipped = false;

        while (true) {
            if (WaitForSingleObject(hProc, 0) == WAIT_OBJECT_0) {
                printf("Brawlhalla exited. Waiting for it to restart...\n");
                if (isClipped) {
                    ClipCursor(nullptr);
                    isClipped = false;
                }
                CloseHandle(hProc);
                break;
            }

            HWND fg = GetForegroundWindow();
            bool hasFocus = (fg == hwnd);
            if (hasFocus && !isClipped) {
                RECT clip = GetClientScreenRect(hwnd);
                if (ClipCursor(&clip)) {
                    isClipped = true;
                    printf("Cursor clipped to window.\n");
                }
                else {
                    printf("ClipCursor failed: %u\n", GetLastError());
                }
            }
            else if (!hasFocus && isClipped) {
                ClipCursor(nullptr);
                isClipped = false;
                printf("Cursor unclipped.\n");
            }

            Sleep(50);
        }
    }

    return 0;
}


