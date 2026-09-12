# C++ Pro Toolbox

A small Windows desktop utility app built with [raylib](https://www.raylib.com/), combining six
lightweight tools behind a single sidebar-navigated window:

- **Word Counter** — live word/character counts, with Open/Save file dialogs
- **Calculator** — expression evaluator supporting `+ - * / ( )` and precedence
- **Countdown Timer** — presets, custom minutes, Start/Pause/Reset
- **Unit & Currency Converter** — Length, Weight, Temperature, Currency
- **Colour Picker (eyedropper)** — sample any pixel on screen, copy hex to clipboard
- **App Launcher** — a search bar that launches apps by name (e.g. "word" → `WINWORD.EXE`)

## Project layout

| File               | Purpose                                                                 |
|--------------------|--------------------------------------------------------------------------|
| `main.cpp`         | App entry point, UI rendering and state for all six tools               |
| `win32helpers.h`  | Shared declarations for the helper functions in `dialogs.cpp` / `system_utils.cpp` |
| `dialogs.cpp`      | Native Windows Open/Save file dialogs                                   |
| `system_utils.cpp` | Win32 helpers: eyedropper pixel sampling, global mouse state, process launching, clipboard |
| `icon.rc`          | Windows resource script embedding the app icon                          |
| `appicon.ico`      | App icon                                                                 |

## Building

This project targets **Windows** (it uses `windows.h`, `commdlg.h`, and `ShellExecute`).

You'll need:

- A C++17-capable compiler (MinGW-w64/g++, or MSVC)
- [raylib](https://www.raylib.com/) headers and library (`raylib.h` under `include/`, plus `libraylib.a` / `raylib.lib`)
- The Windows resource compiler (`windres`, or MSVC's `rc.exe`) to build `icon.rc`

### Building with `make`

A `Makefile` is included and works with g++ or clang++ (MinGW-w64):

```sh
make                 # builds toolbox.exe with g++ (the default)
make CXX=clang++     # or build with clang instead, no editing required
make clean           # remove build artifacts
```

It compiles `icon.rc` via `windres`, compiles each `.cpp` file, and links against the libraries
listed above. To point it at a different raylib install location, override `CXXFLAGS`/`LDLIBS`:

```sh
make CXXFLAGS="-std=c++17 -O2 -IC:/raylib/include" LDLIBS="-LC:/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lcomdlg32"
```

### Building manually (no `make`)

```sh
windres icon.rc -O coff -o icon.res

g++ -std=c++17 -O2 -I include \
    main.cpp dialogs.cpp system_utils.cpp icon.res \
    -o toolbox.exe \
    -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lcomdlg32
```

Notes on linking:

- `-lshell32` is required for `ShellExecuteA` (App Launcher).
- `-lgdi32` is required for `GetPixel`/`GetDC` (Colour Picker eyedropper) — usually already
  linked in raylib's own dependency chain, but list it explicitly if you hit unresolved symbols.
- `-lcomdlg32` is required for `GetOpenFileNameA`/`GetSaveFileNameA` (Word Counter dialogs).

### Visual Studio / MSVC

Add all four `.cpp` files to your project, link against `raylib.lib`, `shell32.lib`,
`gdi32.lib`, `user32.lib`, and `comdlg32.lib`, and compile `icon.rc` as a resource file
(Visual Studio does this automatically if it's added to the project).

## Fonts

The app loads a system TTF at startup for crisp, anti-aliased text (raylib's built-in default
font is a small bitmap font that looks blocky at larger sizes). `LoadUIFont()` in `main.cpp`
tries a list of common font locations in order — Segoe UI / Arial on Windows, San Francisco /
Arial on macOS, DejaVu Sans / Liberation Sans / Noto Sans on common Linux distros — and uses
the first one it finds. If none of them exist on the machine it's running on, it falls back to
raylib's built-in default font automatically, so the app still runs (just with blockier text).

To add another fallback location or reorder the search, edit the `candidates` list in
`LoadUIFont()`. To ship a font instead of relying on the OS having one, place a `.ttf` next to
the executable and add its relative path to the front of that list.

## Notes & known limitations

- **Currency conversion uses a static, hand-entered exchange-rate snapshot** — this app makes
  no network requests, so rates won't update over time. See the comment above
  `g_categories` in `main.cpp` for where to wire up a live FX API (e.g. via WinHTTP) if needed.
- **App Launcher** resolves typed queries against a small built-in alias list (Word, Excel,
  PowerPoint, Notepad, Calculator, Paint, Explorer, Chrome, Command Prompt); anything not in
  that list is passed straight to `ShellExecute`, so a full path or a PATH-resolvable exe name
  also works.
- **Colour Picker** samples the screen at the OS level (`GetPixel`/`GetCursorPos`) and detects
  the confirming click via `GetAsyncKeyState`, so picking works even while hovering another
  application window — not just inside the app itself.

## Controls

- **F11** — toggle fullscreen (any tool)
- **Ctrl+A** — select all text (Word Counter)
- **Enter** — evaluate (Calculator), launch (App Launcher)
- **Esc** — cancel an in-progress colour pick
