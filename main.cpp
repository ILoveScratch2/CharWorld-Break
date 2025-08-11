#include <windows.h>
#include <vector>
#include <algorithm>
#include <TlHelp32.h>
#include <iostream>
#include <fstream>
using namespace std;

// 全局变量
std::vector<HWND> g_targetWindows;  // 目标窗口列表
std::vector<HWND> g_hostWindows;    // 宿主窗口列表
DWORD g_targetPid = 0;              // 目标进程ID
const UINT_PTR TIMER_ID = 1;        // 定时器ID
HHOOK kbdHook = 0;
static const wchar_t* className = L"HostWindowClass";
bool topMost = true;
RECT g_rectWindowBeforeFullscreen = {0};
bool g_bFullscreen = false;

// 宿主窗口数据结构
struct HostData {
    HWND hTarget;
    LONG_PTR originalStyle;
    LONG_PTR originalExStyle;
};

// 宿主窗口消息处理
LRESULT CALLBACK HostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    HostData* pData = reinterpret_cast<HostData*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    
    switch (msg) {
    case WM_SIZE: {
        if (pData && pData->hTarget && IsWindow(pData->hTarget)) {
            RECT rect;
            GetClientRect(hWnd, &rect);
            SetWindowPos(pData->hTarget, NULL, 0, 0, rect.right, rect.bottom, SWP_NOZORDER);
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == TIMER_ID && pData && pData->hTarget && IsWindow(pData->hTarget)) {
            // 强制维持窗口样式和父子关系
            LONG_PTR currentStyle = GetWindowLongPtr(pData->hTarget, GWL_STYLE);
            bool styleChanged = false;
            
            // 确保是子窗口样式
            if ((currentStyle & WS_CHILD) == 0) {
                currentStyle |= WS_CHILD;
                styleChanged = true;
            }
            
            // 移除弹出窗口样式
            if (currentStyle & WS_POPUP) {
                currentStyle &= ~WS_POPUP;
                styleChanged = true;
            }
            
            if (styleChanged) {
                SetWindowLongPtr(pData->hTarget, GWL_STYLE, currentStyle);
            }
            
            // 确保正确的父子关系
            if (GetParent(pData->hTarget) != hWnd) {
                SetParent(pData->hTarget, hWnd);
            }
            
            // 移除顶层窗口属性
            if (GetWindowLongPtr(pData->hTarget, GWL_EXSTYLE) & WS_EX_TOPMOST) {
                SetWindowPos(pData->hTarget, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
        }
        break;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            g_bFullscreen = !g_bFullscreen;
            
            if (g_bFullscreen) {
                GetWindowRect(hWnd, &g_rectWindowBeforeFullscreen);
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                SetWindowPos(hWnd, HWND_TOP, 
                            0, 0, 
                            GetSystemMetrics(SM_CXSCREEN), 
                            GetSystemMetrics(SM_CYSCREEN), 
                            SWP_FRAMECHANGED);
            } else {
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
    case WM_DESTROY: {
        if (pData) {
            // 恢复目标窗口的原始样式
            if (pData->hTarget && IsWindow(pData->hTarget)) {
                SetParent(pData->hTarget, NULL);
                SetWindowLongPtr(pData->hTarget, GWL_STYLE, pData->originalStyle);
                SetWindowLongPtr(pData->hTarget, GWL_EXSTYLE, pData->originalExStyle);
                ShowWindow(pData->hTarget, SW_RESTORE);
            }
            delete pData;
        }
        KillTimer(hWnd, TIMER_ID);
        PostQuitMessage(0);
        break;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// 创建宿主窗口
HWND CreateHostWindow(HWND hTarget) {
    // 保存目标窗口原始样式
    HostData* pData = new HostData();
    pData->hTarget = hTarget;
    pData->originalStyle = GetWindowLongPtr(hTarget, GWL_STYLE);
    pData->originalExStyle = GetWindowLongPtr(hTarget, GWL_EXSTYLE);

    // 创建窗口
    HWND hHost = CreateWindowExW(
        0, className, L"Host Window",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, NULL, NULL, GetModuleHandle(NULL), NULL
    );

    // 存储数据结构
    SetWindowLongPtr(hHost, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pData));
    return hHost;
}

// 嵌入目标窗口到宿主
void EmbedWindow(HWND hTarget, HWND hHost) {
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
    RECT rect;
    GetClientRect(hHost, &rect);
    SetWindowPos(hTarget, NULL, 0, 0, rect.right, rect.bottom,
        SWP_NOZORDER | SWP_FRAMECHANGED);
}

LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    return FALSE;
}

DWORD WINAPI KeyHookThreadProc(LPVOID lpParameter) {
    HMODULE hModule = GetModuleHandle(NULL);
    while (true) {
        kbdHook = (HHOOK)SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)HookProc, hModule, 0);
        Sleep(25);
        UnhookWindowsHookEx(kbdHook);
    }
    return 0;
}

void k() {
    HWND hTarget = 0;
    while (!hTarget) {
        hTarget = FindWindowA("DIBFullViewClass", "");
        Sleep(50);
    }
    
    HWND hHost = CreateHostWindow(hTarget);
    ShowWindow(hHost, SW_SHOW);
    SetWindowPos(hHost, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(hTarget, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    EmbedWindow(hTarget, hHost);
    
    // 设置定时器维持窗口关系
    SetTimer(hHost, TIMER_ID, 100, NULL);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (!IsWindow(hTarget)) {
            DestroyWindow(hHost);
            PostQuitMessage(0);
            break;
        }
        SetWindowPos(hHost, (topMost ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

DWORD WINAPI conIn(LPVOID lpParameter) {
    while (true) {
        int ini = 0;
        cout << "欢迎使用程序\n本程序开源，使用MPL2.0协议\n";
        cout << "使用说明：挂在后台，控屏后自动窗口化控屏窗口，按下ESC切换全屏\n";
        cout << "注意：黑色控制台请勿关闭，关闭程序时请关闭此控制台\n";
        cout << "\n[1] 项目主页\n[2] " << (topMost ? "关闭" : "开启") << "窗口置顶\n>";
        cin >> ini;
        if (ini == 1) {
            system("start https://gitee.com/zhc9968/ZFSJ_PJ/"); 
        } else if (ini == 2) {
            topMost = !topMost; 
            cout << "窗口置顶状态已切换: " << (topMost ? "开启" : "关闭") << endl;
        } else {
            cout << "请输入有效的选项";
        }
        Sleep(3000);
        system("cls"); 
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CreateThread(NULL, 0, KeyHookThreadProc, NULL, 0, NULL);
    
    // 注册窗口类
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEX) };
        wc.lpfnWndProc = HostWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = className;
        wc.hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
        RegisterClassExW(&wc);
        registered = true;
    }
    
    CreateThread(NULL, 0, conIn, NULL, 0, NULL);
    
    while (true) {
        k();
    }
    return 0;
}
