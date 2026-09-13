#pragma once
// win32helpers.h
//
// Shared declarations for the small Win32 helper functions implemented in
// dialogs.cpp and system_utils.cpp. main.cpp includes this instead of
// forward-declaring each function itself, so both call sites and
// implementations stay in sync.

#include <string>

// ---------------------------------------------------------------------------
// dialogs.cpp -- native Windows file-picker dialogs (used by Word Counter's
// Save As... / Open File buttons).
// ---------------------------------------------------------------------------

// Opens the "Select a Text File to Load" dialog. Returns the chosen path,
// or an empty string if the user cancelled.
std::string OpenFileDialog();

// Opens the "Save As..." dialog. Returns the chosen path (with .txt
// appended if the user didn't type an extension), or an empty string if
// the user cancelled.
std::string SaveFileDialog();

// ---------------------------------------------------------------------------
// system_utils.cpp -- OS-level helpers that go beyond what raylib exposes:
// system-wide pixel sampling, global mouse state, process launching, and
// clipboard access.
// ---------------------------------------------------------------------------

// Samples the screen pixel currently under the OS cursor (not just inside
// our window), regardless of which window has focus. Returns false if the
// sample failed. Powers the Colour Picker's eyedropper.
bool GetPixelColorAtCursor(int& outR, int& outG, int& outB);

// Reads the physical left-mouse-button state directly from the OS via
// GetAsyncKeyState, rather than raylib's window-scoped mouse state. This is
// what lets the eyedropper detect a "confirm" click even while the cursor
// is hovering some other application.
bool IsLeftMouseButtonDownGlobal();

// Launches an application or file via ShellExecute (e.g. "WINWORD.EXE", a
// full path, or anything resolvable on PATH). Returns true on success.
// Powers the App Launcher.
bool LaunchApplication(const std::string& target);

// Copies plain text to the system clipboard. Returns true on success.
// Used by the Colour Picker's "Copy Hex" / "Copy RGB" buttons.
bool CopyTextToClipboard(const std::string& text);

// Looks up the current UTC offset (in hours, e.g. 8.0 or -5.5) for an IANA
// timezone name (e.g. "Asia/Shanghai") via a live network request to
// worldtimeapi.org. Returns false on any network failure -- callers should
// fall back to a static approximate offset when that happens. Powers the
// World Clock's lookup for cities that aren't one of the built-in presets.
bool FetchUtcOffsetForTimezone(const std::string& ianaTz, double& outOffsetHours);