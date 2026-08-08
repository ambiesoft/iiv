#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <string>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_CLIPBOARD = WM_APP + 2;
constexpr UINT ID_TRAY_OPEN = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;
constexpr UINT ID_TRAY = 2001;

NOTIFYICONDATAW g_nid{};
HWND g_hwnd = nullptr;

void ShowTrayMenu()
{
    POINT pt{};
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"iiv_view で開く");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"終了");

    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(menu);
}

bool ClipboardHasImage()
{
    if (!OpenClipboard(g_hwnd))
        return false;

    bool result = IsClipboardFormatAvailable(CF_BITMAP) ||
                  IsClipboardFormatAvailable(CF_DIB) ||
                  IsClipboardFormatAvailable(CF_DIBV5);

    CloseClipboard();
    return result;
}

void NotifyImageCopied()
{
    g_nid.uFlags = NIF_INFO;
    wcscpy_s(g_nid.szInfoTitle, L"iiv");
    wcscpy_s(g_nid.szInfo, L"画像をクリップボードにコピーしました");
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void OpenViewer()
{
    // 初期実装では iiv_view.exe を同じフォルダから起動する。
    // 後で設定ファイルからビューアと起動方式を読み込む。
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring path(exePath);
    auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
        return;

    path.resize(pos + 1);
    path += L"iiv_view.exe";

    SHELLEXECUTEINFOW sei{ sizeof(sei) };
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpFile = path.c_str();
    sei.nShow = SW_SHOWNORMAL;

    ShellExecuteExW(&sei);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        AddClipboardFormatListener(hwnd);
        return 0;

    case WM_CLIPBOARDUPDATE:
        if (ClipboardHasImage())
        {
            NotifyImageCopied();
            OpenViewer();
        }
        return 0;

    case WM_TRAY:
        if (lParam == WM_RBUTTONUP)
            ShowTrayMenu();
        else if (lParam == WM_LBUTTONDBLCLK)
            OpenViewer();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_TRAY_OPEN:
            OpenViewer();
            return 0;
        case ID_TRAY_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        RemoveClipboardFormatListener(hwnd);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    const wchar_t CLASS_NAME[] = L"iiv_mon_window";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(
        0, CLASS_NAME, L"iiv_mon",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr);

    if (!g_hwnd)
        return 1;

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = ID_TRAY;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"iiv - Clipboard Image Viewer");

    Shell_NotifyIconW(NIM_ADD, &g_nid);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
