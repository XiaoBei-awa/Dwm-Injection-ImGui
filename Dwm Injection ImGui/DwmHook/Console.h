#pragma once
#include <Windows.h>
#include <stdio.h>

// 控制台输出句柄
static HANDLE g_hConsoleOutput = INVALID_HANDLE_VALUE;

// 重定向的文件流
static FILE* g_fpStdOut = nullptr;

// 加载控制台
void LoadConsole()
{
	// 分配控制台
	AllocConsole();

	// 获取控制台输出句柄
	g_hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

	// 将标准输出重定向到空设备 (屏蔽原进程的输出)
	freopen_s(&g_fpStdOut, "NUL", "w", stdout);

	// 获取控制台窗口句柄
	HWND hConsoleWnd = GetConsoleWindow();

	if (hConsoleWnd)
	{
		// 设置控制台窗口样式 添加 WS_EX_NOACTIVATE
		LONG_PTR exStyle = GetWindowLongPtr(hConsoleWnd, GWL_EXSTYLE);
		exStyle |= WS_EX_NOACTIVATE;   // 无法激活
		SetWindowLongPtr(hConsoleWnd, GWL_EXSTYLE, exStyle);
	}
}

// 卸载控制台
void UnLoadConsole()
{
	// 关闭重定向的文件流
	if (g_fpStdOut)
	{
		fclose(g_fpStdOut);
		g_fpStdOut = nullptr;
	}

	// 释放控制台
	FreeConsole();
	g_hConsoleOutput = INVALID_HANDLE_VALUE;
}

// 输出日志
void log(const char* format, ...)
{
	if (g_hConsoleOutput == INVALID_HANDLE_VALUE) 
		return;

	char buffer[1024];

	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	DWORD written;
	WriteConsoleA(g_hConsoleOutput, buffer, (DWORD)strlen(buffer), &written, NULL);
}