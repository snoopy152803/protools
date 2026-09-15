#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Explicitly use wide strings (LPCWSTR) to avoid character-set errors
    LPCWSTR lpText = L"The application is currently being compiled or is temporarily unavailable.\n\nPlease try again in a few moments.";
    LPCWSTR lpCaption = L"Application Unavailable";
    
    // Explicitly call the Unicode version of the message box function
    MessageBoxW(
        NULL,
        lpText,
        lpCaption,
        MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND
    );

    return 0;
}
