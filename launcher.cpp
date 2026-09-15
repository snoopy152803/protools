#include <windows.h>
#include <filesystem>

namespace fs = std::filesystem;

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    // Find the folder containing this launcher.exe
    wchar_t exePath[MAX_PATH];

    GetModuleFileNameW(
        nullptr,
        exePath,
        MAX_PATH
    );

    fs::path programFolder = fs::path(exePath).parent_path();

    fs::path toolbox = programFolder / "toolbox.exe";
    fs::path unavailable = programFolder / "unavailable.exe";

    // 1. toolbox.exe exists → launch it
    if (fs::exists(toolbox)) {
        ShellExecuteW(
            nullptr,
            L"open",
            toolbox.wstring().c_str(),
            nullptr,
            programFolder.wstring().c_str(),
            SW_SHOWNORMAL
        );

        return 0;
    }

    // 2. toolbox.exe missing, but unavailable.exe exists → launch it
    if (fs::exists(unavailable)) {
        ShellExecuteW(
            nullptr,
            L"open",
            unavailable.wstring().c_str(),
            nullptr,
            programFolder.wstring().c_str(),
            SW_SHOWNORMAL
        );

        return 0;
    }

    // 3. Both are missing → show error
    MessageBoxW(
        nullptr,
        L"PLease reinstall the program. Files are missing.",
        L"Program Unavailable",
        MB_OK | MB_ICONERROR | MB_SETFOREGROUND
    );

    return 1;
}