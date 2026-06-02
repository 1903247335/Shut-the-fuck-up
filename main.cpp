/**
 * Shut-the-fuck-up - Windows 客户端
 *
 * 功能：后台无窗口运行，轮询远程控制服务器，按指令周期性恢复/断开网络。
 * 编译：Visual Studio 2022，Release | x64，SubSystem = Windows
 *
 * 部署前请修改下方「用户配置」中的 SERVER_HOST。
 */

#include <windows.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <string>
#include <thread>
#include <atomic>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

// =============================================================================
// 用户配置（部署前必改）
// =============================================================================

/** 控制服务器 IP 或域名（必须是 Windows 客户端能访问的地址，不能用 127.0.0.1 除非服务器在本机） */
const char* SERVER_HOST = "YOUR_SERVER_IP";

/** 控制服务器端口，需与 server/server.py 一致 */
const int SERVER_PORT = 8000;

/** 开机自启动写入注册表 Run 的项名称，可在 regedit 中查看/删除 */
const wchar_t* APP_REG_NAME = L"ShutTheFuckUp";

/** 注册表自启动路径（一般无需修改） */
const wchar_t* AUTO_START_REG_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// =============================================================================
// 内部状态
// =============================================================================

std::atomic<bool> running(true);    // false 时各工作线程退出
std::atomic<bool> startMode(false); // true = 服务器下发了 start，Worker 开始脉冲循环

const wchar_t* HIDDEN_WND_CLASS = L"ShutTheFuckUpHiddenWnd";

void ReconnectNetwork();
void DisconnectNetwork();

/** 可中断睡眠：每 200ms 检查 running，便于关机时快速退出 */
void InterruptibleSleep(DWORD totalMs)
{
    const DWORD step = 200;
    DWORD elapsed = 0;
    while (running.load() && elapsed < totalMs)
    {
        DWORD chunk = (totalMs - elapsed < step) ? (totalMs - elapsed) : step;
        Sleep(chunk);
        elapsed += chunk;
    }
}

/** 请求退出：停止线程并恢复网络（关机/注销时调用） */
void RequestShutdown()
{
    if (!running.exchange(false))
        return;
    ReconnectNetwork();
}

// =============================================================================
// 隐藏消息窗口：接收 WM_ENDSESSION，实现关机优雅退出
// =============================================================================

LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_QUERYENDSESSION:
        return TRUE;

    case WM_ENDSESSION:
        if (wParam)
            RequestShutdown();
        return 0;

    case WM_CLOSE:
        RequestShutdown();
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND CreateHiddenMessageWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HiddenWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = HIDDEN_WND_CLASS;

    if (!RegisterClassExW(&wc))
        return NULL;

    return CreateWindowExW(
        0, HIDDEN_WND_CLASS, L"", 0,
        0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInstance, NULL);
}

// =============================================================================
// HTTP GET（WinHTTP）
// =============================================================================

std::string HttpGet(const std::wstring& path)
{
    std::string result;

    HINTERNET hSession = WinHttpOpen(
        L"Shut-the-fuck-up",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        std::wstring(SERVER_HOST, SERVER_HOST + strlen(SERVER_HOST)).c_str(),
        (INTERNET_PORT)SERVER_PORT, 0);

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    WinHttpReceiveResponse(hRequest, NULL);

    DWORD size = 0;
    do {
        DWORD read = 0;
        WinHttpQueryDataAvailable(hRequest, &size);
        if (!size) break;

        char* buffer = new char[size + 1];
        ZeroMemory(buffer, size + 1);
        WinHttpReadData(hRequest, buffer, size, &read);
        result.append(buffer, read);
        delete[] buffer;
    } while (size > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

/** 解析 /get_state 响应，响应中含 "start" 子串即为运行态 */
bool ParseStartState(const std::string& res)
{
    return res.find("start") != std::string::npos;
}

/** 解析 /get_interval 响应中的 interval 字段，失败时默认 5 秒 */
int GetInterval()
{
    std::string res = HttpGet(L"/get_interval");

    size_t pos = res.find("interval");
    if (pos == std::string::npos)
        return 5;

    size_t colon = res.find(":", pos);
    size_t end = res.find("}", colon);

    return atoi(res.substr(colon + 1, end - colon - 1).c_str());
}

// =============================================================================
// 开机自启动（当前用户 HKCU Run）
// =============================================================================

std::wstring GetExePath()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return path;
}

bool SetAutoStart(bool enable)
{
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTO_START_REG_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return false;

    bool ok = true;
    if (enable)
    {
        std::wstring cmd = L"\"" + GetExePath() + L"\"";
        if (RegSetValueExW(hKey, APP_REG_NAME, 0, REG_SZ,
            (const BYTE*)cmd.c_str(),
            (DWORD)((cmd.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS)
            ok = false;
    }
    else
    {
        RegDeleteValueW(hKey, APP_REG_NAME);
    }

    RegCloseKey(hKey);
    return ok;
}

void EnsureAutoStart()
{
    SetAutoStart(true);
}

// =============================================================================
// 网络控制（IpRenewAddress / IpReleaseAddress，作用于本机所有枚举到的网卡）
// =============================================================================

void DisconnectNetwork()
{
    ULONG len = sizeof(IP_INTERFACE_INFO);
    PIP_INTERFACE_INFO pInfo = (PIP_INTERFACE_INFO)malloc(len);

    if (GetInterfaceInfo(pInfo, &len) == ERROR_INSUFFICIENT_BUFFER)
    {
        free(pInfo);
        pInfo = (PIP_INTERFACE_INFO)malloc(len);
    }

    if (GetInterfaceInfo(pInfo, &len) == NO_ERROR)
    {
        for (ULONG i = 0; i < pInfo->NumAdapters; i++)
            IpReleaseAddress(&pInfo->Adapter[i]);
    }

    free(pInfo);
}

void ReconnectNetwork()
{
    ULONG len = sizeof(IP_INTERFACE_INFO);
    PIP_INTERFACE_INFO pInfo = (PIP_INTERFACE_INFO)malloc(len);

    if (GetInterfaceInfo(pInfo, &len) == ERROR_INSUFFICIENT_BUFFER)
    {
        free(pInfo);
        pInfo = (PIP_INTERFACE_INFO)malloc(len);
    }

    if (GetInterfaceInfo(pInfo, &len) == NO_ERROR)
    {
        for (ULONG i = 0; i < pInfo->NumAdapters; i++)
            IpRenewAddress(&pInfo->Adapter[i]);
    }

    free(pInfo);
}

// =============================================================================
// 工作线程：start 模式下循环「联网 → 等待 interval → 断网」
// =============================================================================

void Worker()
{
    bool wasRunning = false;

    while (running)
    {
        if (!startMode)
        {
            // 从运行态切回 stop 时，恢复网络
            if (wasRunning)
            {
                ReconnectNetwork();
                wasRunning = false;
            }
            InterruptibleSleep(2000);
            continue;
        }

        wasRunning = true;

        // 先联网，再拉 interval（远程服务器必须先有网才能访问）
        ReconnectNetwork();
        InterruptibleSleep(2000);
        if (!running) break;

        int interval = GetInterval();

        InterruptibleSleep((DWORD)(interval * 1000));
        if (!running) break;

        DisconnectNetwork();
    }
}

// =============================================================================
// 状态监听线程：每 2 秒轮询 /get_state
// =============================================================================

void StateWatcher()
{
    while (running)
    {
        std::string res = HttpGet(L"/get_state");
        // 请求失败时保持原状态，避免断网窗口内误判为 stop
        if (!res.empty())
            startMode = ParseStartState(res);

        InterruptibleSleep(2000);
    }
}

// =============================================================================
// 程序入口
// =============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    FreeConsole();

    EnsureAutoStart();

    HWND hwnd = CreateHiddenMessageWindow(hInstance);
    if (!hwnd)
        return 1;

    std::thread t1(Worker);
    std::thread t2(StateWatcher);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    running = false;
    if (t1.joinable()) t1.join();
    if (t2.joinable()) t2.join();

    return 0;
}
