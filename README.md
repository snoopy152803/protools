# C++ Pro Toolbox

A small Windows desktop utility app built with [raylib](https://www.raylib.com/), combining six
lightweight tools behind a single sidebar-navigated window, with a light/dark theme toggle and
switchable UI font:

- **Word Counter** — live word/character counts, with Open/Save file dialogs
- **Calculator** — expression evaluator supporting `+ - * / ( )` and precedence
- **Time** — four sub-tools in one tab:
  - **Countdown** — presets, custom minutes, Start/Pause/Reset
  - **Stopwatch** — Start/Stop, Lap, Reset
  - **Alarm** — set a daily HH:MM, rings (visually) when reached, keeps checking even while
    you're using a different tool
  - **World Clock** — Local + UTC by default; add any of ~35 built-in major cities (e.g.
    "Beijing") and it looks up the current UTC offset live over the network, falling back to a
    static approximate offset if there's no connection
- **Unit & Currency Converter** — Length, Weight, Temperature, Currency
- **Colour Picker (eyedropper)** — sample any pixel on screen, adjust with R/G/B sliders, copy
  hex or `rgb(...)` to clipboard
- **App Launcher** — a search bar that launches apps by name (e.g. "word" → `WINWORD.EXE`)

## Project layout

| File               | Purpose                                                                 |
|--------------------|--------------------------------------------------------------------------|
| `main.cpp`         | App entry point, UI rendering and state for all tools, theming, and font switching |
| `win32helpers.h`   | Shared declarations for the helper functions in `dialogs.cpp` / `system_utils.cpp` |
| `dialogs.cpp`      | Native Windows Open/Save file dialogs                                   |
| `system_utils.cpp` | Win32 helpers: eyedropper pixel sampling, global mouse state, process launching, clipboard, live timezone lookup |
| `icon.rc`          | Windows resource script embedding the app icon                          |
| `appicon.ico`      | App icon                                                                 |

## Building

This project targets **Windows** (it uses `windows.h`, `commdlg.h`, `ShellExecute`, and `WinHTTP`).

You'll need:

- A C++17-capable compiler (MinGW-w64/g++, clang++, or MSVC)
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
make CXXFLAGS="-std=c++17 -O2 -IC:/raylib/include" LDLIBS="-LC:/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lcomdlg32 -lwinhttp"
```

### Building manually (no `make`)

```sh
windres icon.rc -O coff -o icon.res

g++ -std=c++17 -O2 -I include \
    main.cpp dialogs.cpp system_utils.cpp icon.res \
    -o toolbox.exe \
    -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lcomdlg32 -lwinhttp
```

Notes on linking:

- `-lshell32` is required for `ShellExecuteA` (App Launcher).
- `-lgdi32` is required for `GetPixel`/`GetDC` (Colour Picker eyedropper) — usually already
  linked in raylib's own dependency chain, but list it explicitly if you hit unresolved symbols.
- `-lcomdlg32` is required for `GetOpenFileNameA`/`GetSaveFileNameA` (Word Counter dialogs).
- `-lwinhttp` is required for the World Clock's live timezone lookup.

### Visual Studio / MSVC

Add all four `.cpp` files to your project, link against `raylib.lib`, `shell32.lib`,
`gdi32.lib`, `user32.lib`, `comdlg32.lib`, and `winhttp.lib`, and compile `icon.rc` as a resource
file (Visual Studio does this automatically if it's added to the project).

## Theme & fonts

A theme toggle and a font-switch button both live at the bottom of the sidebar.

- **Light/Dark**: click "Switch to Light" / "Switch to Dark" at any time — every screen re-renders
  from a `Palette` struct (see `MakeDarkPalette()` / `MakeLightPalette()` in `main.cpp`), so
  there's one place to tweak colours for either theme.
- **Font**: click the "Font: ..." button to cycle through a small built-in list of font
  "families" (Sans, Mono, Serif, Comic Sans). Each family is actually a list of candidate file
  paths searched across platforms — e.g. "Sans" tries Segoe UI and Arial on Windows, San
  Francisco and Arial on macOS, then DejaVu Sans / Liberation Sans / Noto Sans on common Linux
  distros — and uses whichever file it finds first. This is also how the very first font load
  works at startup (raylib's built-in default font is a small bitmap font that looks blocky at
  larger sizes, hence loading a real TTF instead).

  If none of the candidates in a family exist on the current machine, that family silently falls
  back to raylib's default font rather than crashing.

  To add a font family, add an entry to `g_fontChoices` in `main.cpp` with its own candidate path
  list. To ship a font instead of relying on the OS having one, place a `.ttf` next to the
  executable and add its relative path to the front of a candidate list.

## Notes & known limitations

- **Currency conversion uses a static, hand-entered exchange-rate snapshot** — see the comment
  above `g_categories` in `main.cpp` for where to wire up a live FX API if needed (the World
  Clock's `FetchUtcOffsetForTimezone` in `system_utils.cpp` shows the WinHTTP pattern to follow).
- **World Clock lookup** matches typed city names against a ~35-entry table (`g_cityTimezones`
  in `main.cpp`) mapping city → IANA timezone name. A match triggers a live HTTP request to
  `worldtimeapi.org` for the current (DST-correct) UTC offset; if that request fails, it falls
  back to a static approximate offset (which does *not* account for DST). A city not in the
  table at all isn't looked up — add an entry to `g_cityTimezones` to support it. Note the
  lookup blocks the UI thread briefly while the request is in flight.
- **Alarm** only tracks a single daily HH:MM (no AM/PM toggle — it's 24-hour) and checks every
  frame regardless of which tool is currently visible, so it will still ring while you're using
  the Calculator, for example.
- **App Launcher** resolves typed queries against a small built-in alias list (Word, Excel,
  PowerPoint, Notepad, Calculator, Paint, Explorer, Chrome, Command Prompt); anything not in
  that list is passed straight to `ShellExecute`, so a full path or a PATH-resolvable exe name
  also works.
- **Colour Picker** samples the screen at the OS level (`GetPixel`/`GetCursorPos`) and detects
  the confirming click via `GetAsyncKeyState`, so picking works even while hovering another
  application window — not just inside the app itself. The R/G/B sliders are disabled while
  actively picking, since the live eyedropper sample would otherwise fight with manual dragging.

## Controls

- **F11** — toggle fullscreen (any tool)
- **Ctrl+A** — select all text (Word Counter)
- **Enter** — evaluate (Calculator), submit (World Clock's add-a-city field, App Launcher)
- **Esc** — cancel an in-progress colour pick
