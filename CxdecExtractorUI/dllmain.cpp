#include <Windows.h>
#include <commdlg.h>
#include "resource.h"

#include <Shlwapi.h>
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib,"comdlg32.lib")

/// <summary>
/// 最大路径
/// </summary>
constexpr size_t MaxPath = 1024u;

typedef void (WINAPI* tExtractFunc)(const wchar_t* packageName);
static tExtractFunc g_ExtractPackage = nullptr;

/// <summary>
/// Opens a native Windows file picker and extracts the selected XP3 archive.
/// The extractor core resolves packages relative to the game executable, so
/// the selected archive must be in the same folder as the running game.
/// </summary>
static void BrowseAndExtract(HWND hwnd)
{
    if (!g_ExtractPackage)
    {
        ::MessageBoxW(hwnd, L"CxdecExtractor.dll is not initialized.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t selectedFile[MaxPath] = { 0 };
    wchar_t gameExePath[MaxPath] = { 0 };
    wchar_t gameDirectory[MaxPath] = { 0 };

    if (!::GetModuleFileNameW(nullptr, gameExePath, MaxPath))
    {
        ::MessageBoxW(hwnd, L"Unable to determine the game directory.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    ::lstrcpynW(gameDirectory, gameExePath, MaxPath);
    ::PathRemoveFileSpecW(gameDirectory);

    OPENFILENAMEW dialog = { 0 };
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd;
    dialog.lpstrFilter = L"XP3 archives (*.xp3)\0*.xp3\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = selectedFile;
    dialog.nMaxFile = MaxPath;
    dialog.lpstrInitialDir = gameDirectory;
    dialog.lpstrTitle = L"Select an XP3 archive from the game folder";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (!::GetOpenFileNameW(&dialog))
        return;

    wchar_t selectedDirectory[MaxPath] = { 0 };
    ::lstrcpynW(selectedDirectory, selectedFile, MaxPath);
    ::PathRemoveFileSpecW(selectedDirectory);

    if (::_wcsicmp(selectedDirectory, gameDirectory) != 0)
    {
        ::MessageBoxW(
            hwnd,
            L"Select an XP3 archive located in the same folder as the game executable.",
            L"Wrong folder",
            MB_OK | MB_ICONWARNING);
        return;
    }

    const wchar_t* fileName = ::PathFindFileNameW(selectedFile);
    g_ExtractPackage(fileName);
}

/// <summary>
/// 主窗体消息循环
/// </summary>
/// <param name="hwnd">窗口句柄</param>
/// <param name="msg">消息</param>
/// <param name="wParam"></param>
/// <param name="lParam"></param>
INT_PTR CALLBACK ExtractorDialogWindProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
    switch (msg)
    {
        case WM_DROPFILES:
        {
            HDROP hDrop = (HDROP)wParam;
            wchar_t fullName[MaxPath];
            //只获取第一项
            if (UINT strLen = ::DragQueryFileW(hDrop, 0u, fullName, MaxPath))
            {
                DWORD attr = ::GetFileAttributesW(fullName);
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_ARCHIVE) == FILE_ATTRIBUTE_ARCHIVE)
                {
                    const wchar_t* fileName = ::PathFindFileNameW(fullName);
                    g_ExtractPackage(fileName);
                }
                ::DragFinish(hDrop);
            }
            return TRUE;
        }
        case WM_COMMAND:
        {
            if (LOWORD(wParam) == IDC_BrowseXp3 && HIWORD(wParam) == BN_CLICKED)
            {
                BrowseAndExtract(hwnd);
                return TRUE;
            }
            break;
        }
        case WM_CLOSE:
        {
            ::DestroyWindow(hwnd);
            return TRUE;
        }
        case WM_DESTROY:
        {
            ::PostQuitMessage(0);
            return TRUE;
        }
    }
    return FALSE;
}


/// <summary>
/// 窗口代码
/// </summary>
/// <param name="hInstance">模块基地址</param>
DWORD WINAPI WinExtractorEntry(LPVOID hInstance) 
{
    HWND hwnd = ::CreateDialogParamW((HINSTANCE)hInstance, MAKEINTRESOURCEW(IDD_MainForm), NULL, ExtractorDialogWindProc, 0u);
    ::ShowWindow(hwnd, SW_NORMAL);

    MSG msg{ };
    while (BOOL ret = ::GetMessageW(&msg, NULL, 0u, 0u))
    {
        if (ret == -1) 
        {
            return -1;
        }
        else
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }
    return 0u;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            constexpr const wchar_t CoreDllNameW[] = L"CxdecExtractor.dll";

            wchar_t moduleFullPath[MaxPath];
            DWORD strLen = ::GetModuleFileNameW(hModule, moduleFullPath, MaxPath);
            wchar_t* dllName = ::PathFindFileNameW(moduleFullPath);
            memcpy(dllName, CoreDllNameW, sizeof(CoreDllNameW));

            if (HMODULE coreBase = ::LoadLibraryW(moduleFullPath))
            {
                g_ExtractPackage = (tExtractFunc)::GetProcAddress(coreBase, "ExtractPackage");
                if (HANDLE hThread = ::CreateThread(NULL, 0u, WinExtractorEntry, hModule, 0u, NULL))
                {
                    ::CloseHandle(hThread);
                }
            }
            else
            {
                ::MessageBoxW(nullptr, L"CxdecExtractor.dll加载失败", L"错误", MB_OK);
            }
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH: 
            break;
        case DLL_PROCESS_DETACH:
        {
            break;
        }
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void Dummy(){}