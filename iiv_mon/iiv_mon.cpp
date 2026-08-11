#include "iiv_mon.h"
#include "Settngs.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

using namespace Ambiesoft;
using namespace Ambiesoft::stdosd;

constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_CLIPBOARD = WM_APP + 2;
constexpr UINT ID_TRAY_OPEN = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;
constexpr UINT ID_TRAY = 2001;

NOTIFYICONDATAW g_nid{};
HWND g_hwnd = nullptr;
DWORD ClipImageData::lastTick_ = 0;

void ShowTrayMenu()
{
    POINT pt{};
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Open with iiv_view");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(menu);
}

std::wstring getTempImagePath()
{
	std::wstring tempDir = stdCombinePath(
        stdGetParentDirectory(stdGetModuleFileName()).c_str(),
		L"temp");
    if (!CreateDirectory(tempDir.c_str(), nullptr))
    {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
        {
            MessageBox(g_hwnd, stdFormat(L"Failed to create temp directory: %s", tempDir.c_str()).c_str(), L"Error", MB_ICONERROR);
            return L"";
		}
    }
    std::wstring tempPath = GetUnexistingFile(
		tempDir.c_str(),
        L"iiv", L".bmp");
    return tempPath;
}

// Compute MD5 of a file using CryptoAPI
static bool ComputeFileMD5(const std::wstring& filePath, std::wstring& outHex)
{
    HANDLE hFile;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool success = false;
    do {
        outHex.clear();
        hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;

        if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
            break;

        if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
            break;

        const DWORD bufSize = 64 * 1024;
        std::vector<BYTE> buffer(bufSize);
        DWORD bytesRead = 0;
        bool bBreak = false;
        while (ReadFile(hFile, buffer.data(), bufSize, &bytesRead, nullptr) && bytesRead > 0)
        {
            if (!CryptHashData(hHash, buffer.data(), bytesRead, 0))
            {
                bBreak = true;
                break;
            }
        }
        if(bBreak)
			break;

        BYTE rgbHash[16];
        DWORD cbHash = sizeof(rgbHash);
        if (!CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0))
            break;

        static const wchar_t hexDigits[] = L"0123456789abcdef";
        outHex.reserve(cbHash * 2);
        for (DWORD i = 0; i < cbHash; ++i)
        {
            BYTE b = rgbHash[i];
            outHex.push_back(hexDigits[b >> 4]);
            outHex.push_back(hexDigits[b & 0x0F]);
        }

        success = true;
	} while (false);

    if (hHash)
        CryptDestroyHash(hHash);
    if (hProv)
        CryptReleaseContext(hProv, 0);
    CloseHandle(hFile);
    return success;
}

bool GetClipboardImage4(ClipImageData* imageData)
{
    std::wstring error;
    std::wstring tempImagePath = getTempImagePath();
    if(tempImagePath.empty())
		return false;   

    if (!SaveClipboardImageToFile(tempImagePath.c_str(), &error))
    {
        MessageBox(g_hwnd, error.c_str(), L"Error", MB_ICONERROR);
        return false;
    }

    const std::wstring ext = stdGetFileExtension(tempImagePath.c_str());

    /*
    Pseudocode / Plan:
    1. Open the saved file at 'tempImagePath' for reading.
    2. Initialize CryptoAPI context with CryptAcquireContext for hashing.
    3. Create an MD5 hash object with CryptCreateHash.
    4. Read the file in a loop (e.g. 64KB chunks) and feed each chunk to CryptHashData.
    5. Finalize and retrieve the raw hash bytes via CryptGetHashParam (HP_HASHVAL).
    6. Convert the raw bytes to a lowercase hexadecimal wide string.
    7. Store the resulting MD5 hex string into 'imageData->md5_' (if available).
    8. Clean up CryptoAPI objects and close the file handle.
    9. If any step fails, perform cleanup and report failure.
    */
    // Calculate MD5 and store it (if computation succeeds)
    std::wstring md5;
    if (!ComputeFileMD5(tempImagePath, md5))
    {
        MessageBox(g_hwnd, L"Failed to get md5", L"Error", MB_ICONERROR);
        return false;
    }

    long fileSize = stdGetFileSize(tempImagePath.c_str());

    std::wstring newFilename = stdFormat(L"%d-%s%s",
        fileSize,
        md5.c_str(),
        ext.c_str());

	// Rename the file to include size and MD5
	std::wstring newFilePath = stdCombinePath(
        stdGetParentDirectory(tempImagePath), newFilename);
    if (!MoveFile(tempImagePath.c_str(), newFilePath.c_str()))
    {
        MessageBox(g_hwnd, L"Failed to rename file", L"Error", MB_ICONERROR);
		return false;
    }

    imageData->imagePath_ = newFilePath;
    return true;
}
bool GetClipboardImage3(ClipImageData* imageData)
{
    if (!OpenClipboard(g_hwnd))
        return false;
    ClipboardCloser clipboardCloser;

    if (IsClipboardFormatAvailable(CF_BITMAP))
    {
        imageData->format_ = CF_BITMAP;
        return GetClipboardImage4(imageData);
    }
    if (IsClipboardFormatAvailable(CF_DIB))
    {
        imageData->format_ = CF_DIB;
        return GetClipboardImage4(imageData);
    }
    if (IsClipboardFormatAvailable(CF_DIBV5))
    {
        imageData->format_ = CF_DIBV5;
        return GetClipboardImage4(imageData);
    }

    if (IsClipboardFormatAvailable(CF_HDROP))
    {
        HANDLE hDrop = GetClipboardData(CF_HDROP);
        if (hDrop)
        {
            UINT fileCount = DragQueryFileW((HDROP)hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < fileCount; ++i)
            {
                wchar_t filePath[MAX_PATH]{};
                DragQueryFileW((HDROP)hDrop, i, filePath, MAX_PATH);
                DTRACE(filePath);
                std::wstring ext = filePath;
                auto pos = ext.find_last_of(L'.');
                if (pos != std::wstring::npos)
                    ext = ext.substr(pos + 1);
                else
                    ext.clear();
                if (_wcsicmp(ext.c_str(), L"png") == 0 ||
                    _wcsicmp(ext.c_str(), L"jpg") == 0 ||
                    _wcsicmp(ext.c_str(), L"jpeg") == 0 ||
                    _wcsicmp(ext.c_str(), L"bmp") == 0 ||
                    _wcsicmp(ext.c_str(), L"gif") == 0)
                {
                    imageData->format_ = CF_HDROP;
                    imageData->imagePath_ = filePath;
                    return true;
                }
            }
        }
    }
    return false;
}
bool GetClipboardImage2(ClipImageData* imageData)
{
    if (!GetClipboardImage3(imageData))
        return false;

    return true;
}
bool GetClipboardImage(ClipImageData* imageData)
{
    if (!GetClipboardImage2(imageData))
        return false;

    static ClipImageData lastImageData;

    if(imageData->format_==0)
        return false;
    
    DWORD currentTick = GetTickCount();
    if (imageData->format_ == lastImageData.format_ &&
        imageData->imagePath_ == lastImageData.imagePath_ &&
        (currentTick - lastImageData.lastTick_) < 1000)
    {
        return false;
    }
    lastImageData = *imageData;
    lastImageData.lastTick_ = currentTick;
    return true;
}


void NotifyImageCopied()
{
    g_nid.uFlags = NIF_INFO;
    wcscpy_s(g_nid.szInfoTitle, L"iiv");
    wcscpy_s(g_nid.szInfo, L"OOOAAA");
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

std::wstring getViewer()
{
    std::wstring viewerPath = getSettings().getViewer();
    if (viewerPath.empty())
    {
        std::wstring iivViewPath = stdCombinePath(
            stdGetParentDirectory(stdGetModuleFileName()),
            L"iiv_view.exe");
        return iivViewPath;
    }
    // return L"C:/local/ImageGlass/ImageGlass.exe";
    // return L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";
	return viewerPath;
}
void OpenViewer(const wchar_t* imagePath = nullptr)
{
    std::wstring exe = getViewer();
    std::wstring arg = stdFormat(L"%s", 
        imagePath ? stdAddDQIfNecessary(imagePath).c_str() : L"");

	// MessageBox(nullptr, stdFormat(L"exe: %s\narg: %s", exe.c_str(), arg.c_str()).c_str(), L"Debug", MB_OK);

    SHELLEXECUTEINFOW sei{ sizeof(sei) };
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpFile = exe.c_str();
    sei.lpParameters = arg.c_str();
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
    {
        ClipImageData clipImageData;
        if (GetClipboardImage(&clipImageData))
        {
            NotifyImageCopied();
            OpenViewer(clipImageData.imagePath_.c_str());
        }
        return 0;
    }
    break;

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
    if (!getSettings().loadSettings())
    {
        MessageBox(nullptr, L"Failed to load settings", L"Error", MB_ICONERROR);
		return 1;
    }

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
