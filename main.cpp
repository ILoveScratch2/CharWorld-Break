
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <winuser.h>
#include <vector>
#include <algorithm>
#include <TlHelp32.h>
#include <iostream>
#pragma comment(lib, "User32.lib")
using namespace std;

// 全局变量
static const wchar_t* className = L"HostWindowClass";
bool topMost = true;
RECT g_rectWindowBeforeFullscreen = {0};
bool g_bFullscreen = false;
bool g_gotTarget = false;
HWND hTarget = 0;
HWND hHost = 0;
bool g_Debug = false;
bool conProc = true;
bool blockShut = true;
LPCWSTR rst = L"ShutdownBlock";
bool g_bExitProgram = false;
const UINT WM_EXIT_PROGRAM = WM_USER + 100;  // 退出程序消息

// 宿主窗口消息处理
void db_o(string s)
{
	if (g_Debug)
	{
		cout << s << endl;
	}
	return;
}

LRESULT CALLBACK HostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_SIZE:
			{
				InvalidateRect(hTarget, NULL, TRUE);
				UpdateWindow(hTarget);
				if (IsWindow(hTarget)&&g_gotTarget)
				{
					RECT rect;
					GetClientRect(hWnd, &rect);
					SetWindowPos(hTarget, NULL, 0, 0, rect.right, rect.bottom, SWP_NOZORDER | SWP_FRAMECHANGED);
				}
				SetWindowPos(hHost, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
				break;
			}
		case WM_CLOSE:
			return 0;
		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE)
			{
				g_bFullscreen = !g_bFullscreen;

				if (g_bFullscreen)
				{
					GetWindowRect(hWnd, &g_rectWindowBeforeFullscreen);
					SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
					SetWindowPos(hWnd, HWND_TOP,
					             0, 0,
					             GetSystemMetrics(SM_CXSCREEN),
					             GetSystemMetrics(SM_CYSCREEN),
					             SWP_FRAMECHANGED);
				}
				else
				{
					SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
					SetWindowPos(hWnd, NULL,
					             g_rectWindowBeforeFullscreen.left,
					             g_rectWindowBeforeFullscreen.top,
					             g_rectWindowBeforeFullscreen.right - g_rectWindowBeforeFullscreen.left,
					             g_rectWindowBeforeFullscreen.bottom - g_rectWindowBeforeFullscreen.top,
					             SWP_FRAMECHANGED | SWP_NOZORDER);
				}
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
			// 新增：处理退出程序消息
		case WM_EXIT_PROGRAM:
			if (blockShut)
			{
				HMODULE hUser32 = LoadLibrary("user32.dll");
				if (hUser32)
				{
					typedef BOOL (WINAPI *ShutdownBlockReasonFunc)(HWND);
					ShutdownBlockReasonFunc pShutdownBlockReasonDestroy =
					    (ShutdownBlockReasonFunc)GetProcAddress(hUser32, "ShutdownBlockReasonDestroy");
					if (pShutdownBlockReasonDestroy)
					{
						pShutdownBlockReasonDestroy(hWnd);
					}
					FreeLibrary(hUser32);
				}
			}
			DestroyWindow(hWnd);
			break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}


// 解锁键盘功能
void UnlockKeyboard()
{
	HMODULE hDll = LoadLibraryA("C:/Program Files (x86)/广州海光/海耘云教学系统/redhooks.dll");
	if (!hDll)
	{
		cout << "DLL加载失败! 错误代码: " << GetLastError() << endl;
		return;
	}

	typedef BOOL (__stdcall *FN_ENABLE_INPUT_MESSAGE)();
	FN_ENABLE_INPUT_MESSAGE EnableInputMessage =
	    (FN_ENABLE_INPUT_MESSAGE)GetProcAddress(hDll, "_EnableInputMessage");

	if (!EnableInputMessage)
	{
		cout << "函数获取失败! 错误代码: " << GetLastError() << endl;
		FreeLibrary(hDll);
		return;
	}

	BOOL result = EnableInputMessage();
	cout << "解锁" << (result ? "成功" : "失败") << endl;
	FreeLibrary(hDll);
}


// 创建宿主窗口
HWND CreateHostWindow(HWND hTarget)
{
	// 创建窗口
	HWND hHost = CreateWindowExW(
	                 0, className, L"Host Window",
	                 WS_POPUP | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
	                 800, 600, NULL, NULL, GetModuleHandle(NULL), NULL
	             );
	HMENU hMenu = GetSystemMenu(hHost, FALSE);
	EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
	// 存储目标窗口句柄
	SetWindowLongPtr(hHost, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(hTarget));
	return hHost;
}

// 嵌入目标窗口到宿主
void EmbedWindow()
{
	if (!IsWindow(hTarget) || !IsWindowVisible(hTarget) || GetParent(hTarget) != NULL)
		return;

	// 修改窗口样式
	LONG_PTR style = GetWindowLongPtr(hTarget, GWL_STYLE);
	LONG_PTR exStyle = GetWindowLongPtr(hTarget, GWL_EXSTYLE);

	// 移除不需要的样式
	style &= ~(WS_POPUP);
	style |= WS_CHILD;

	// 移除顶层窗口样式
	exStyle &= ~(WS_EX_TOPMOST);

	SetWindowLongPtr(hTarget, GWL_STYLE, style);
	SetWindowLongPtr(hTarget, GWL_EXSTYLE, exStyle);

	// 设置父子关系
	SetParent(hTarget, hHost);

	// 调整尺寸
	InvalidateRect(hTarget, NULL, TRUE);
	UpdateWindow(hTarget);
	RECT rect;
	GetClientRect(hHost, &rect);
	SetWindowPos(hTarget, NULL, rect.left, rect.top, rect.right, rect.bottom, SWP_NOZORDER | SWP_FRAMECHANGED);
	SetWindowPos(hHost, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
	UnlockKeyboard();

}

std::vector<HWND> FindWindowsByClass(LPCWSTR className)
{
	std::vector<HWND> windows;
	HWND hWnd = NULL;
	while ((hWnd = FindWindowExW(NULL, hWnd, className, NULL)) != NULL)
	{
		windows.push_back(hWnd);
	}

	return windows;
}

// 为窗口添加可调整大小的边框
void AddResizableBorder(HWND hWnd)
{
	if (!IsWindow(hWnd)) return;
	LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
	style |= WS_THICKFRAME;
	SetWindowLongPtr(hWnd, GWL_STYLE, style);
	SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
	             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}



DWORD WINAPI conIn(LPVOID lpParameter)
{
	while (!g_bExitProgram)
	{
		int ini=0;
		cout << "欢迎使用CharWorld-Breaker\n本程序开源，使用GPL 3.0协议\n使用说明：挂在后台，老师控屏后自动窗口化控屏窗口，\n注意：使用中黑色控制台请勿关闭，如需关闭程序请关闭黑色控制台\n";
		cout << "\n\n\n\n\n[1]项目主页\n[2](" << (topMost?"关闭":"开启") << ")窗口置顶\n[3]" << (conProc?"开启":"关闭") << "进程劫持(阻止启动)\n[4]" << (blockShut?"关闭":"开启") << "关机阻止\n[5]解锁键盘\n[6]退出程序\n>";
		cin >> ini;
		if (ini==1)
		{
			system("start https://github.com/ILoveScratch2/");
		}
		else if (ini==2)
		{
			topMost = !topMost;
		}
		else if (ini==3)
		{
			conProc = !conProc;
			if (conProc)
			{
				system("reg delete \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\REDAgent.exe\" /f");
			}
			else
			{
				system("reg add \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\REDAgent.exe\" /v Debugger /t REG_SZ /d \"C:\\Windows\\System32\\alg.exe\" /f");
				system("taskkill /f /im REDAgent.exe");
			}
		}
		else if (ini==4)
		{
			blockShut = !blockShut;
			if (blockShut)
			{
				HMODULE hUser32 = LoadLibrary("user32.dll");
				if (hUser32)
				{
					typedef BOOL (WINAPI *ShutdownBlockReasonFunc)(HWND, LPCWSTR);
					ShutdownBlockReasonFunc pShutdownBlockReasonCreate =
					    (ShutdownBlockReasonFunc)GetProcAddress(hUser32, "ShutdownBlockReasonCreate");
					if (pShutdownBlockReasonCreate)
					{
						pShutdownBlockReasonCreate(hHost, rst);
					}
					FreeLibrary(hUser32);
				}
			}
			else
			{
				HMODULE hUser32 = LoadLibrary("user32.dll");
				if (hUser32)
				{
					typedef BOOL (WINAPI *ShutdownBlockReasonFunc)(HWND, LPCWSTR);
					ShutdownBlockReasonFunc pShutdownBlockReasonDestroy =
					    (ShutdownBlockReasonFunc)GetProcAddress(hUser32, "ShutdownBlockReasonDestroy");
					if (pShutdownBlockReasonDestroy)
					{
						pShutdownBlockReasonDestroy(hHost, rst);
					}
					FreeLibrary(hUser32);
				}
			}
		}
		else if (ini == 5)
		{
			UnlockKeyboard();
		}
		else if (ini==6)
		{
			g_bExitProgram = true;
			PostMessage(hHost, WM_EXIT_PROGRAM, 0, 0);
			return 0;
		}
		else if (ini==49)
		{
			system("cls");
			g_Debug = true;
			return 0;
		}
		else
		{
			cout << "请输入正确的内容\n";
		}
		cout << "执行完成";
		Sleep(1500);
		system("cls");
	}
	return 0;
}

DWORD WINAPI findWD(LPVOID lpParameter)
{
	while (!g_bExitProgram)   // 加退出条件
	{
		while (!IsWindow(hTarget) && !g_bExitProgram)    // 添加退出条件
		{
			hTarget = FindWindowA("DIBFullViewClass", "");
			Sleep(50);
		}
		if (g_bExitProgram) break;  // 检查退出标志
		db_o("finded");
		g_gotTarget = true;
		ShowWindow(hHost, SW_SHOW);
		EmbedWindow();
		while(g_gotTarget && !g_bExitProgram)   // 添加退出条件
		{
			Sleep(250);
		}
	}
	return 0;
}

DWORD WINAPI findFalse(LPVOID lpParameter)
{
	while (!g_bExitProgram)
	{
		if (!IsWindow(hTarget)&&g_gotTarget)
		{
			g_gotTarget = false;
			ShowWindow(hHost, SW_HIDE);
			db_o("hide");
		}
		Sleep(500);
	}
	return 0;
}

DWORD WINAPI setThickFrame(LPVOID lpParameter)
{
	while (!g_bExitProgram)
	{
		vector<HWND> monitorWindows = FindWindowsByClass(L"RedEagle.Monitor");
		for (HWND hWnd : monitorWindows)
		{
			AddResizableBorder(hWnd);
		}
		Sleep(1000);
	}
	return 0;
}

BOOL WINAPI ConsoleHandler(DWORD signal)
{
	if (signal == CTRL_CLOSE_EVENT ||
	        signal == CTRL_LOGOFF_EVENT ||
	        signal == CTRL_SHUTDOWN_EVENT)
	{
		return TRUE; // 阻止关闭
	}
	return FALSE;
}




void hideclose()
{
	HWND hwnd = GetConsoleWindow();
	if (hwnd != NULL)
	{
		HMENU hMenu = GetSystemMenu(hwnd, FALSE);
		if (hMenu != NULL)
		{
			EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
			DrawMenuBar(hwnd);
		}
		LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
		LONG_PTR EXstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
		style &= ~WS_THICKFRAME;  // 移除可调整边框样式
		style &= ~WS_MAXIMIZEBOX; // 移除最大化按钮
		EXstyle |= WS_EX_TOPMOST;
		SetWindowLongPtr(hwnd, GWL_STYLE, style);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, EXstyle);
		// 强制重绘窗口
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
		             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// 只保留必要的线程
	CreateThread(NULL, 0, setThickFrame, NULL, 0, NULL);
	hideclose();
	WNDCLASSEXW wc = { sizeof(WNDCLASSEX) };
	wc.lpfnWndProc = HostWndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = className;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	RegisterClassExW(&wc);
	CreateThread(NULL, 0, conIn, NULL, 0, NULL);
	hHost = CreateHostWindow(hTarget);
	ShowWindow(hHost, SW_HIDE);
	CreateThread(NULL, 0, findWD, NULL, 0, NULL);
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	CreateThread(NULL, 0, findFalse, NULL, 0, NULL);
	HMODULE hUser32 = LoadLibrary("user32.dll");
	if (hUser32)
	{
		typedef BOOL (WINAPI *ShutdownBlockReasonFunc)(HWND, LPCWSTR);
		ShutdownBlockReasonFunc pShutdownBlockReasonCreate =
		    (ShutdownBlockReasonFunc)GetProcAddress(hUser32, "ShutdownBlockReasonCreate");
		if (pShutdownBlockReasonCreate)
		{
			pShutdownBlockReasonCreate(hHost, rst);
		}
		FreeLibrary(hUser32);
	}
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		db_o("msg");
		if (g_gotTarget)
		{
			if (!IsWindow(hTarget))
			{
				g_gotTarget = false;
				ShowWindow(hHost, SW_HIDE);
				db_o("hide");
			}
			SetWindowPos(hHost, (topMost?HWND_TOPMOST:HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			// 维持窗口关系
			if (IsWindow(hTarget) && IsWindow(hHost))
			{
				LONG_PTR style = GetWindowLongPtr(hTarget, GWL_STYLE);
				style &= ~WS_POPUP;
				style |= WS_CHILD;
				SetWindowLongPtr(hTarget, GWL_STYLE, style);
				SetWindowPos(hTarget, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				if (GetParent(hTarget) != hHost)
				{
					SetParent(hTarget, hHost);
					db_o("setp");

				}
			}
		}
		// 检查退出标志
		if (g_bExitProgram)
		{
			break;
		}
	}
	// 清理资源
	if (blockShut)
	{
		HMODULE hUser32 = LoadLibrary("user32.dll");
		if (hUser32)
		{
			typedef BOOL (WINAPI *ShutdownBlockReasonFunc)(HWND);
			ShutdownBlockReasonFunc pShutdownBlockReasonDestroy =
			    (ShutdownBlockReasonFunc)GetProcAddress(hUser32, "ShutdownBlockReasonDestroy");
			if (pShutdownBlockReasonDestroy)
			{
				pShutdownBlockReasonDestroy(hHost);
			}
			FreeLibrary(hUser32);
		}
	}
	return 0;
}
