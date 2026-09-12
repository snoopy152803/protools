#include <windows.h>
#include <string>

// ---------------------------------------------------------------------------
// Eyedropper support.
// GetCursorPos / GetPixel are both OS-wide APIs -- they work no matter which
// window has focus, which is what makes a real "pick a colour from anywhere
// on screen" eyedropper possible from an unfocused app window.
// ---------------------------------------------------------------------------
bool GetPixelColorAtCursor(int& outR, int& outG, int& outB) {
    POINT pt;
    if (!GetCursorPos(&pt)) return false;

    HDC hdc = GetDC(NULL); // NULL = device context for the whole screen
    if (!hdc) return false;

    COLORREF color = GetPixel(hdc, pt.x, pt.y);
    ReleaseDC(NULL, hdc);

    if (color == CLR_INVALID) return false;

    outR = GetRValue(color);
    outG = GetGValue(color);
    outB = GetBValue(color);
    return true;
}

// Raylib's mouse state only reflects clicks while our window has input focus.
// GetAsyncKeyState reads the physical button state directly from the OS, so
// this still detects a click even while the cursor is hovering some other
// application (needed to "confirm" an eyedropper pick anywhere on screen).
bool IsLeftMouseButtonDownGlobal() {
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

// ---------------------------------------------------------------------------
// App launcher support.
// ---------------------------------------------------------------------------
bool LaunchApplication(const std::string& target) {
    if (target.empty()) return false;
    HINSTANCE result = ShellExecuteA(NULL, "open", target.c_str(), NULL, NULL, SW_SHOWNORMAL);
    // ShellExecute returns a value > 32 on success.
    return reinterpret_cast<INT_PTR>(result) > 32;
}

// ---------------------------------------------------------------------------
// Clipboard support (used by the colour picker's "Copy hex" button).
// ---------------------------------------------------------------------------
bool CopyTextToClipboard(const std::string& text) {
    if (!OpenClipboard(NULL)) return false;

    EmptyClipboard();

    size_t bytes = text.size() + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) { CloseClipboard(); return false; }

    void* dest = GlobalLock(hMem);
    memcpy(dest, text.c_str(), bytes);
    GlobalUnlock(hMem);

    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
    // Note: ownership of hMem passes to the system once SetClipboardData
    // succeeds -- we must not free it ourselves.
    return true;
}