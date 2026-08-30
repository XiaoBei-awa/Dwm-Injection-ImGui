#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <string>

using namespace std;
bool EnableDebugPrivilege();
bool InjectDllByPID(DWORD pid, const wchar_t* dllFullPath);

int main()
{
    // ====================== 自定义 DLL 路径 ======================
    const wchar_t* TARGET_DLL_PATH = L"C:\\DwmHook.dll";
    // =========================================================

    wcout << L"DWM DLL Injector (CreateRemoteThread + LoadLibraryW)" << endl;
    wcout << L"Target DLL: " << TARGET_DLL_PATH << endl << endl;

    // 提升权限
    wcout << L"[+] Enabling SeDebugPrivilege..." << endl;
    if (!EnableDebugPrivilege())
    {
        wcout << L"Failed! Please run this program as ADMINISTRATOR." << endl;
        system("pause");
        return 1;
    }
    wcout << L"OK" << endl << endl;

    // 枚举所有 dwm.exe
    wcout << L"[+] Enumerating dwm.exe processes..." << endl;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        wcout << L"CreateToolhelp32Snapshot failed: " << GetLastError() << endl;
        system("pause");
        return 1;
    }

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe))
    {
        CloseHandle(hSnapshot);
        wcout << L"Process32First failed" << endl;
        system("pause");
        return 1;
    }

    bool hasAnySuccess = false;
    int dwmCount = 0;

    do
    {
        if (_wcsicmp(pe.szExeFile, L"dwm.exe") == 0)
        {
            dwmCount++;
            wcout << endl << L"Found dwm.exe, PID = " << pe.th32ProcessID << endl;

            if (InjectDllByPID(pe.th32ProcessID, TARGET_DLL_PATH))
            {
                wcout << L"  Inject SUCCESS" << endl;
                hasAnySuccess = true;
            }
            else
            {
                wcout << L"  Inject FAILED" << endl;
            }
        }
    } while (Process32Next(hSnapshot, &pe));

    CloseHandle(hSnapshot);

    // 结果输出
    wcout << endl << L"==============================" << endl;
    if (dwmCount == 0)
    {
        wcout << L"No dwm.exe process found." << endl;
    }
    else if (!hasAnySuccess)
    {
        wcout << L"Failed to load or initialize DLL." << endl;
        wcout << L"This probably means that a LUT file is malformed or that DWM got updated." << endl;
    }
    else
    {
        wcout << L"Injection finished. At least one success." << endl;
    }

    return 0;
}

// 提升 SeDebugPrivilege 调试权限
bool EnableDebugPrivilege()
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        wcout << L"OpenProcessToken failed: " << GetLastError() << endl;
        return false;
    }

    LUID luid{};
    // 不使用 A 后缀，自动匹配当前项目字符集
    if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid))
    {
        CloseHandle(hToken);
        wcout << L"LookupPrivilegeValue failed: " << GetLastError() << endl;
        return false;
    }

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr))
    {
        CloseHandle(hToken);
        wcout << L"AdjustTokenPrivileges failed: " << GetLastError() << endl;
        return false;
    }

    DWORD err = GetLastError();
    CloseHandle(hToken);

    if (err != ERROR_SUCCESS)
    {
        wcout << L"Enable debug privilege failed, error code: " << err << endl;
        return false;
    }
    return true;
}

// 向指定 PID 进程注入 DLL
bool InjectDllByPID(DWORD pid, const wchar_t* dllFullPath)
{
    // 打开进程
    HANDLE hProcess = OpenProcess(
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_CREATE_THREAD,
        FALSE,
        pid
    );
    if (!hProcess)
    {
        wcout << L"  OpenProcess failed, error: " << GetLastError() << endl;
        return false;
    }

    // 宽字符路径长度计算
    size_t pathLen = (wcslen(dllFullPath) + 1) * sizeof(wchar_t);

    // 在目标进程分配内存
    LPVOID pRemoteMem = VirtualAllocEx(
        hProcess,
        nullptr,
        pathLen,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (!pRemoteMem)
    {
        wcout << L"  VirtualAllocEx failed, error: " << GetLastError() << endl;
        CloseHandle(hProcess);
        return false;
    }

    // 写入 DLL 路径字符串
    if (!WriteProcessMemory(hProcess, pRemoteMem, dllFullPath, pathLen, nullptr))
    {
        wcout << L"  WriteProcessMemory failed, error: " << GetLastError() << endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 获取 LoadLibraryW 函数地址
    FARPROC pLoadLibrary = GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    if (!pLoadLibrary)
    {
        wcout << L"  GetProcAddress LoadLibraryW failed: " << GetLastError() << endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 创建远程线程执行 LoadLibraryW
    HANDLE hRemoteThread = CreateRemoteThread(
        hProcess,
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)pLoadLibrary,
        pRemoteMem,
        0,
        nullptr
    );
    if (!hRemoteThread)
    {
        wcout << L"  CreateRemoteThread failed, error: " << GetLastError() << endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 等待线程执行完毕
    WaitForSingleObject(hRemoteThread, INFINITE);

    // 获取线程退出码
    DWORD exitCode = 0;
    GetExitCodeThread(hRemoteThread, &exitCode);

    // 清理资源
    CloseHandle(hRemoteThread);
    VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    // LoadLibraryW 成功返回模块句柄
    if (exitCode == 0)
    {
        wcout << L"  Remote LoadLibrary failed (exit code 0)" << endl;
        return false;
    }

    return true;
}
