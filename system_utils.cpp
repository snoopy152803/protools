#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>

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

// ---------------------------------------------------------------------------
// World Clock support: live UTC-offset lookup for cities that aren't one of
// the app's hardcoded presets.
//
// main.cpp maps a typed city name (e.g. "Beijing") to an IANA timezone name
// (e.g. "Asia/Shanghai") -- that mapping is app data, so it lives there. This
// function does the actual network fetch: it queries worldtimeapi.org for
// that IANA timezone and pulls the "utc_offset" field out of the JSON
// response with a plain substring search (avoids pulling in a full JSON
// parser for one field). Returns false on any network failure, non-200
// response, or unparseable body -- callers should fall back to a static
// approximate offset when that happens.
// ---------------------------------------------------------------------------
bool FetchUtcOffsetForTimezone(const std::string& ianaTz, double& outOffsetHours) {
    HINTERNET hSession = WinHttpOpen(L"CppProToolbox/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    // 10-second timeouts so a flaky connection doesn't freeze the UI thread.
    WinHttpSetTimeouts(hSession, 10000, 10000, 10000, 10000);

    HINTERNET hConnect = WinHttpConnect(hSession, L"worldtimeapi.org", INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    std::string path = "/api/timezone/" + ianaTz;
    std::wstring wpath(path.begin(), path.end()); // IANA tz names are plain ASCII, safe to widen this way

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(), NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
           && WinHttpReceiveResponse(hRequest, NULL);

    std::string body;
    if (ok) {
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
            std::vector<char> chunk(available);
            DWORD bytesRead = 0;
            if (!WinHttpReadData(hRequest, chunk.data(), available, &bytesRead) || bytesRead == 0) break;
            body.append(chunk.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (!ok || body.empty()) return false;

    // Response looks like: {"...","utc_offset":"+08:00","...",...}
    const std::string key = "\"utc_offset\":\"";
    size_t pos = body.find(key);
    if (pos == std::string::npos) return false;
    pos += key.size();
    if (pos + 6 > body.size()) return false; // need at least "+HH:MM"

    char sign = body[pos];
    if (sign != '+' && sign != '-') return false;
    if (!isdigit((unsigned char)body[pos+1]) || !isdigit((unsigned char)body[pos+2]) ||
        !isdigit((unsigned char)body[pos+4]) || !isdigit((unsigned char)body[pos+5])) return false;

    int hh = (body[pos+1] - '0') * 10 + (body[pos+2] - '0');
    int mm = (body[pos+4] - '0') * 10 + (body[pos+5] - '0');
    double offset = hh + mm / 60.0;
    if (sign == '-') offset = -offset;

    outOffsetHours = offset;
    return true;
}
