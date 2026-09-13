#include "include/raylib.h"
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <stdexcept>

#include "win32helpers.h" // dialogs.cpp + system_utils.cpp declarations

#define COLOR_REF(hex) GetColor((hex << 8) | 0xFF)

// ===========================================================================
// Theme (light / dark)
// ===========================================================================

enum Theme { THEME_DARK = 0, THEME_LIGHT = 1 };

struct Palette {
    Color bg, panelDark, panel, hover, textPrimary, textSecondary, borderIdle,
          accent, textOnAccent, danger, success, warning, dangerDim, successDim,
          hoverLight, selectionHighlight;
};

Palette MakeDarkPalette() {
    Palette p;
    p.bg = COLOR_REF(0x1e1e24);
    p.panelDark = COLOR_REF(0x141419);
    p.panel = COLOR_REF(0x2d2d38);
    p.hover = COLOR_REF(0x3a3a46);
    p.textPrimary = RAYWHITE;
    p.textSecondary = GRAY;
    p.borderIdle = DARKGRAY;
    p.accent = MAROON;
    p.textOnAccent = WHITE;
    p.danger = RED;
    p.success = GREEN;
    p.warning = ORANGE;
    p.dangerDim = COLOR_REF(0x8a3030);
    p.successDim = COLOR_REF(0x2d5a2d);
    p.hoverLight = LIGHTGRAY;
    p.selectionHighlight = GetColor(0x3a7bd588);
    return p;
}

Palette MakeLightPalette() {
    Palette p;
    p.bg = COLOR_REF(0xf4f4f6);
    p.panelDark = COLOR_REF(0xe2e2e7);
    p.panel = COLOR_REF(0xffffff);
    p.hover = COLOR_REF(0xd7d7dd);
    p.textPrimary = COLOR_REF(0x1c1c22);
    p.textSecondary = COLOR_REF(0x5a5a63);
    p.borderIdle = COLOR_REF(0xb9b9c0);
    p.accent = MAROON;
    p.textOnAccent = WHITE;
    p.danger = COLOR_REF(0xb42a3a);
    p.success = COLOR_REF(0x1f7a3a);
    p.warning = COLOR_REF(0xb56a00);
    p.dangerDim = COLOR_REF(0x8a3030);
    p.successDim = COLOR_REF(0x2d5a2d);
    p.hoverLight = COLOR_REF(0xcfcfd6);
    p.selectionHighlight = GetColor(0x3a7bd588);
    return p;
}

Theme g_theme = THEME_DARK;
Palette g_darkPalette;
Palette g_lightPalette;
Palette& CurrentPalette() { return (g_theme == THEME_DARK) ? g_darkPalette : g_lightPalette; }

// ===========================================================================
// Fonts: loading + a switchable list of font "families"
// ===========================================================================

Font g_uiFont;
const float UI_FONT_SPACING = 1.0f;

void UIDrawText(const char* text, int x, int y, int fontSize, Color color) {
    DrawTextEx(g_uiFont, text, Vector2{ (float)x, (float)y }, (float)fontSize, UI_FONT_SPACING, color);
}

int UIMeasureText(const char* text, int fontSize) {
    Vector2 size = MeasureTextEx(g_uiFont, text, (float)fontSize, UI_FONT_SPACING);
    return (int)size.x;
}

// Every existing DrawText(...)/MeasureText(...) call below now transparently
// goes through our custom font instead of raylib's default one.
#define DrawText UIDrawText
#define MeasureText UIMeasureText

// Tries each candidate path in order and returns the first that loads
// successfully, so the app gets a real TTF on Windows, macOS, and Linux
// alike. Falls back to raylib's built-in (blocky) default font only if none
// of the candidates exist on this machine.
Font LoadUIFontFromCandidates(const std::vector<const char*>& candidates, int baseSize) {
    for (const char* path : candidates) {
        Font f = LoadFontEx(path, baseSize, NULL, 0);
        if (f.texture.id != 0) {
            SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
            return f;
        }
    }
    return GetFontDefault();
}

struct FontChoice { const char* label; std::vector<const char*> candidates; };

// Each entry is a "family" with the same style searched across platforms --
// pick whichever file exists first. To add a bundled font instead of relying
// on the OS, just add its relative path to the front of a candidate list.
static std::vector<FontChoice> g_fontChoices = {
    { "Sans (Segoe UI)", {
        "C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\arial.ttf",
        "/System/Library/Fonts/SFNSText.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf", "/Library/Fonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf",
    }},
    { "Mono (Consolas)", {
        "C:\\Windows\\Fonts\\consola.ttf",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    }},
    { "Serif (Georgia)", {
        "C:\\Windows\\Fonts\\georgia.ttf", "C:\\Windows\\Fonts\\times.ttf",
        "/System/Library/Fonts/Supplemental/Georgia.ttf", "/System/Library/Fonts/Supplemental/Times New Roman.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf",
    }},
    { "Comic Sans", {
        "C:\\Windows\\Fonts\\comic.ttf",
        "/usr/share/fonts/truetype/comic-relief/ComicRelief-Regular.ttf",
    }},
};

int g_fontChoiceIndex = 0;

void ReloadFont(int index) {
    if (g_uiFont.texture.id != GetFontDefault().texture.id) UnloadFont(g_uiFont);
    g_fontChoiceIndex = ((index % (int)g_fontChoices.size()) + (int)g_fontChoices.size()) % (int)g_fontChoices.size();
    g_uiFont = LoadUIFontFromCandidates(g_fontChoices[g_fontChoiceIndex].candidates, 96);
}

enum AppMode {
    MODE_WORD_COUNTER = 0,
    MODE_CALCULATOR,
    MODE_TIME,
    MODE_CONVERTER,
    MODE_COLOR_PICKER,
    MODE_LAUNCHER,
    MODE_COUNT
};

enum TimeSubMode {
    TIME_COUNTDOWN = 0,
    TIME_STOPWATCH,
    TIME_ALARM,
    TIME_WORLDCLOCK
};

// ===========================================================================
// Shared helpers
// ===========================================================================

int CountWords(const std::string& str) {
    std::stringstream ss(str);
    std::string word;
    int count = 0;
    while (ss >> word) count++;
    return count;
}

std::vector<std::string> WrapText(const std::string& text, int fontSize, int maxWidth) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string word;
    std::string currentLine = "";

    while (ss >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        if (MeasureText(testLine.c_str(), fontSize) > maxWidth) {
            if (!currentLine.empty()) {
                lines.push_back(currentLine);
                currentLine = word;
            } else {
                lines.push_back(word);
                currentLine = "";
            }
        } else {
            currentLine = testLine;
        }
    }
    if (!currentLine.empty()) lines.push_back(currentLine);
    return lines;
}

// ---------------------------------------------------------------------------
// Small recursive-descent expression evaluator: + - * / ( ) and unary minus.
// ---------------------------------------------------------------------------
struct ExprParser {
    const std::string& s;
    size_t i = 0;

    explicit ExprParser(const std::string& src) : s(src) {}

    void SkipSpace() { while (i < s.size() && s[i] == ' ') i++; }
    char Peek() { SkipSpace(); return i < s.size() ? s[i] : '\0'; }

    double ParseExpression() {
        double value = ParseTerm();
        while (true) {
            char c = Peek();
            if (c == '+') { i++; value += ParseTerm(); }
            else if (c == '-') { i++; value -= ParseTerm(); }
            else break;
        }
        return value;
    }

    double ParseTerm() {
        double value = ParseFactor();
        while (true) {
            char c = Peek();
            if (c == '*') { i++; value *= ParseFactor(); }
            else if (c == '/') {
                i++;
                double denom = ParseFactor();
                if (denom == 0.0) throw std::runtime_error("div0");
                value /= denom;
            } else break;
        }
        return value;
    }

    double ParseFactor() {
        char c = Peek();
        if (c == '+') { i++; return ParseFactor(); }
        if (c == '-') { i++; return -ParseFactor(); }
        if (c == '(') {
            i++;
            double value = ParseExpression();
            if (Peek() != ')') throw std::runtime_error("paren");
            i++;
            return value;
        }
        return ParseNumber();
    }

    double ParseNumber() {
        SkipSpace();
        size_t start = i;
        while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i] == '.')) i++;
        if (start == i) throw std::runtime_error("num");
        return std::stod(s.substr(start, i - start));
    }
};

bool EvaluateExpression(const std::string& expr, double& result) {
    try {
        ExprParser parser(expr);
        result = parser.ParseExpression();
        if (parser.Peek() != '\0') return false;
        return true;
    } catch (...) {
        return false;
    }
}

std::string FormatNumber(double value) {
    std::ostringstream oss;
    oss.precision(10);
    oss << value;
    return oss.str();
}

std::string ToLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

std::string TrimCopy(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    size_t end = s.find_last_not_of(" \t");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

// A reusable single-line numeric text field: handles digit/'.'/backspace
// input when 'active' is true. Returns true if the value changed this frame.
bool NumericFieldInput(std::string& text, bool active, size_t maxLen = 24) {
    if (!active) return false;
    bool changed = false;
    int key = GetCharPressed();
    while (key > 0) {
        bool valid = (key >= '0' && key <= '9') || key == '.' || key == '-';
        if (valid && text.length() < maxLen) { text += (char)key; changed = true; }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) { text.pop_back(); changed = true; }
    return changed;
}

// A horizontal slider (used by the colour picker's R/G/B sliders). Drag
// anywhere on the track or handle to change 'value'. Returns true if it
// changed this frame.
bool DrawAndUpdateSlider(Rectangle bounds, int& value, int minVal, int maxVal,
                          Color trackColor, Color fillColor, Color handleColor,
                          Vector2 mousePos, bool mouseDown) {
    float t = (float)(value - minVal) / (float)(maxVal - minVal);
    float handleX = bounds.x + t * bounds.width;

    DrawRectangleRec(bounds, trackColor);
    DrawRectangleRec(Rectangle{ bounds.x, bounds.y, handleX - bounds.x, bounds.height }, fillColor);
    float handleW = 10.0f;
    DrawRectangleRec(Rectangle{ handleX - handleW / 2, bounds.y - 4, handleW, bounds.height + 8 }, handleColor);

    Rectangle grabZone = { bounds.x - 6, bounds.y - 8, bounds.width + 12, bounds.height + 16 };
    bool changed = false;
    if (mouseDown && CheckCollisionPointRec(mousePos, grabZone)) {
        float rel = (mousePos.x - bounds.x) / bounds.width;
        if (rel < 0.0f) rel = 0.0f;
        if (rel > 1.0f) rel = 1.0f;
        int newVal = minVal + (int)(rel * (maxVal - minVal) + 0.5f);
        if (newVal != value) { value = newVal; changed = true; }
    }
    return changed;
}

// ===========================================================================
// Unit / currency converter data
// ===========================================================================

struct Unit { const char* name; double toBase; };

struct ConverterCategory {
    const char* name;
    std::vector<Unit> units; // ignored for Temperature, which is handled specially
};

// NOTE ON CURRENCY: these rates are a static, hand-entered snapshot (to USD)
// captured when this was written -- this build has no general internet
// integration for currency, so it cannot fetch live FX rates. To make this
// live, replace the lookup below with a real HTTP call (see
// FetchUtcOffsetForTimezone in system_utils.cpp for the WinHTTP pattern used
// by the World Clock) and cache the result.
static std::vector<ConverterCategory> g_categories = {
    { "Length", { {"m", 1.0}, {"km", 1000.0}, {"ft", 0.3048}, {"in", 0.0254}, {"mi", 1609.34} } },
    { "Weight", { {"kg", 1.0}, {"g", 0.001}, {"lb", 0.453592}, {"oz", 0.0283495} } },
    { "Temperature", { {"C", 0}, {"F", 0}, {"K", 0} } }, // factors unused, handled specially
    { "Currency (static rates)", { {"USD", 1.0}, {"EUR", 0.92}, {"GBP", 0.79}, {"JPY", 149.5}, {"AUD", 1.53} } }
};

double ConvertTemperature(const std::string& from, const std::string& to, double v) {
    double celsius;
    if (from == "C") celsius = v;
    else if (from == "F") celsius = (v - 32.0) * 5.0 / 9.0;
    else celsius = v - 273.15; // K

    if (to == "C") return celsius;
    if (to == "F") return celsius * 9.0 / 5.0 + 32.0;
    return celsius + 273.15; // K
}

double ConvertValue(int categoryIdx, int fromIdx, int toIdx, double value) {
    ConverterCategory& cat = g_categories[categoryIdx];
    if (std::string(cat.name) == "Temperature") {
        return ConvertTemperature(cat.units[fromIdx].name, cat.units[toIdx].name, value);
    }
    double base = value * cat.units[fromIdx].toBase;
    return base / cat.units[toIdx].toBase;
}

// ===========================================================================
// World Clock data
// ===========================================================================

struct CityTimezone { const char* city; const char* ianaTz; double approxOffsetHours; };

// A city -> IANA timezone map for the World Clock's "look it up" flow. Not
// exhaustive -- add more entries here as needed. approxOffsetHours is a
// fallback used only if the live network lookup fails (it ignores DST).
static std::vector<CityTimezone> g_cityTimezones = {
    { "beijing", "Asia/Shanghai", 8.0 }, { "shanghai", "Asia/Shanghai", 8.0 },
    { "tokyo", "Asia/Tokyo", 9.0 }, { "seoul", "Asia/Seoul", 9.0 },
    { "singapore", "Asia/Singapore", 8.0 }, { "bangkok", "Asia/Bangkok", 7.0 },
    { "jakarta", "Asia/Jakarta", 7.0 }, { "mumbai", "Asia/Kolkata", 5.5 },
    { "delhi", "Asia/Kolkata", 5.5 }, { "dubai", "Asia/Dubai", 4.0 },
    { "moscow", "Europe/Moscow", 3.0 }, { "istanbul", "Europe/Istanbul", 3.0 },
    { "cairo", "Africa/Cairo", 2.0 }, { "johannesburg", "Africa/Johannesburg", 2.0 },
    { "paris", "Europe/Paris", 1.0 }, { "berlin", "Europe/Berlin", 1.0 },
    { "madrid", "Europe/Madrid", 1.0 }, { "rome", "Europe/Rome", 1.0 },
    { "london", "Europe/London", 0.0 }, { "lisbon", "Europe/Lisbon", 0.0 },
    { "sao paulo", "America/Sao_Paulo", -3.0 }, { "buenos aires", "America/Argentina/Buenos_Aires", -3.0 },
    { "new york", "America/New_York", -5.0 }, { "toronto", "America/Toronto", -5.0 },
    { "chicago", "America/Chicago", -6.0 }, { "mexico city", "America/Mexico_City", -6.0 },
    { "denver", "America/Denver", -7.0 }, { "los angeles", "America/Los_Angeles", -8.0 },
    { "vancouver", "America/Vancouver", -8.0 }, { "auckland", "Pacific/Auckland", 12.0 },
    { "sydney", "Australia/Sydney", 10.0 }, { "melbourne", "Australia/Melbourne", 10.0 },
    { "perth", "Australia/Perth", 8.0 }, { "honolulu", "Pacific/Honolulu", -10.0 },
    { "anchorage", "America/Anchorage", -9.0 },
};

struct WorldClockEntry {
    std::string displayName;
    bool isLocal;      // uses the machine's local time directly (no offset math)
    double offsetHours; // UTC offset, used when !isLocal
    std::string note;  // e.g. "live" / "offline estimate" / "" for Local & UTC
};

std::string FormatClockTime(const struct tm& t) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

std::string FormatClockDate(const struct tm& t) {
    static const char* months[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
    char buf[32];
    snprintf(buf, sizeof(buf), "%d %s", t.tm_mday, months[t.tm_mon >= 0 && t.tm_mon < 12 ? t.tm_mon : 0]);
    return buf;
}

// ===========================================================================
// App launcher data
// ===========================================================================

struct LaunchAlias { const char* label; const char* target; };

static std::vector<LaunchAlias> g_launchAliases = {
    { "Word", "WINWORD.EXE" },
    { "Excel", "EXCEL.EXE" },
    { "PowerPoint", "POWERPNT.EXE" },
    { "Notepad", "notepad.exe" },
    { "Calculator", "calc.exe" },
    { "Paint", "mspaint.exe" },
    { "Explorer", "explorer.exe" },
    { "Chrome", "chrome.exe" },
    { "Command Prompt", "cmd.exe" },
};

// Resolves a typed query against the alias table (matches on the label,
// case-insensitively), falling back to launching the raw text itself so
// power users can type an exe name or full path directly.
std::string ResolveLaunchTarget(const std::string& query) {
    std::string q = ToLowerCopy(query);
    for (auto& alias : g_launchAliases) {
        if (ToLowerCopy(alias.label) == q) return alias.target;
    }
    return query; // fall back to literal text
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    const float baseWidth = 950.0f;
    const float baseHeight = 660.0f;
    InitWindow((int)baseWidth, (int)baseHeight, "C++ Pro Toolbox");
    SetTargetFPS(60);

    g_darkPalette = MakeDarkPalette();
    g_lightPalette = MakeLightPalette();

    // ReloadFont(0) both loads the default font family and sets g_uiFont --
    // avoids duplicating the "try candidates, fall back to default" logic.
    g_uiFont = GetFontDefault(); // placeholder so ReloadFont's unload check is well-defined
    ReloadFont(0);

    AppMode mode = MODE_WORD_COUNTER;

    // --- Word counter state ---
    std::string textInput = "Type or load text here...";
    bool boxSelected = false;
    bool isAllSelected = false;

    // --- Calculator state ---
    std::string calcExpr = "";
    std::string calcDisplay = "0";
    bool calcError = false;

    // --- Time tool state ---
    TimeSubMode timeSubMode = TIME_COUNTDOWN;

    // Countdown
    double timerTotalSeconds = 5 * 60.0;
    double timerRemaining = timerTotalSeconds;
    bool timerRunning = false;
    std::string timerMinutesInput = "5";
    bool timerFieldActive = false;

    // Stopwatch
    double stopwatchElapsed = 0.0;
    bool stopwatchRunning = false;
    std::vector<double> stopwatchLaps;

    // Alarm
    int alarmHour = 7, alarmMinute = 0;
    bool alarmEnabled = false;
    bool alarmRinging = false;
    int alarmLastTriggeredKey = -1;
    bool alarmHourFieldActive = false, alarmMinuteFieldActive = false;
    std::string alarmHourInput = "07", alarmMinuteInput = "00";

    // World clock
    std::vector<WorldClockEntry> worldClocks = {
        { "Local", true, 0.0, "" },
        { "UTC", false, 0.0, "" },
    };
    std::string worldClockSearch = "";
    bool worldClockFieldActive = false;
    std::string worldClockError = "";

    // --- Converter state ---
    int convCategory = 0;
    int convFromUnit = 0;
    int convToUnit = 1;
    std::string convInput = "1";
    bool convFieldActive = false;

    // --- Colour picker state ---
    bool pickerActive = false;
    bool prevGlobalMouseDown = false;
    int pickedR = 255, pickedG = 255, pickedB = 255;
    bool havePickedColor = false;
    int liveR = 255, liveG = 255, liveB = 255;
    std::string copyFeedback = "";
    double copyFeedbackUntil = 0.0;

    // --- Launcher state ---
    std::string launcherQuery = "";
    bool launcherFieldActive = true;
    std::string launcherFeedback = "";
    bool launcherFeedbackOk = true;

    while (!WindowShouldClose()) {
        float currentWidth = (float)GetScreenWidth();
        float currentHeight = (float)GetScreenHeight();

        float scaleW = currentWidth / baseWidth;
        float scaleH = currentHeight / baseHeight;
        float scale = (scaleW < scaleH) ? scaleW : scaleH;
        if (scale < 0.6f) scale = 0.6f;
        if (scale > 2.0f) scale = 2.0f;

        Vector2 mousePos = GetMousePosition();
        Palette& pal = CurrentPalette();

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        // -------------------------------------------------------------
        // Background time-keeping: countdown/stopwatch/alarm all keep
        // running even while a different tool is on screen.
        // -------------------------------------------------------------
        if (timerRunning) {
            timerRemaining -= GetFrameTime();
            if (timerRemaining <= 0.0) { timerRemaining = 0.0; timerRunning = false; }
        }
        if (stopwatchRunning) {
            stopwatchElapsed += GetFrameTime();
        }
        {
            time_t nowT = time(nullptr);
            struct tm localNow = *localtime(&nowT);
            int key = localNow.tm_hour * 60 + localNow.tm_min;
            if (alarmEnabled && !alarmRinging && localNow.tm_hour == alarmHour && localNow.tm_min == alarmMinute && key != alarmLastTriggeredKey) {
                alarmRinging = true;
                alarmLastTriggeredKey = key;
            }
            if (key != (alarmHour * 60 + alarmMinute)) {
                // Once we've moved off the trigger minute, re-arm for tomorrow.
                if (alarmLastTriggeredKey == key) { /* still the trigger minute, leave armed state as-is */ }
            }
        }

        // -------------------------------------------------------------
        // Sidebar
        // -------------------------------------------------------------
        struct NavItem { const char* line1; const char* line2; AppMode mode; };
        static const NavItem navItems[] = {
            { "Word", "Counter", MODE_WORD_COUNTER },
            { "Calculator", nullptr, MODE_CALCULATOR },
            { "Time", nullptr, MODE_TIME },
            { "Unit/Currency", "Converter", MODE_CONVERTER },
            { "Colour", "Picker", MODE_COLOR_PICKER },
            { "App", "Launcher", MODE_LAUNCHER },
        };
        const int navCount = (int)(sizeof(navItems) / sizeof(navItems[0]));

        float sidebarWidth = 170 * scale;
        Rectangle sidebar = { 0, 0, sidebarWidth, currentHeight };

        float navBtnH = 48 * scale;
        float navBtnGap = 10 * scale;
        float navBtnPad = 12 * scale;
        float navStartY = 25 * scale;

        Rectangle navRects[navCount];
        for (int n = 0; n < navCount; n++) {
            navRects[n] = { navBtnPad, navStartY + n * (navBtnH + navBtnGap), sidebarWidth - 2 * navBtnPad, navBtnH };
        }

        // Footer controls: theme toggle + font cycle, pinned to the bottom.
        float footerBtnH = 40 * scale;
        float footerGap = 8 * scale;
        Rectangle themeBtn = { navBtnPad, currentHeight - footerBtnH - navBtnPad, sidebarWidth - 2 * navBtnPad, footerBtnH };
        Rectangle fontBtn = { navBtnPad, themeBtn.y - footerGap - footerBtnH, sidebarWidth - 2 * navBtnPad, footerBtnH };

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int n = 0; n < navCount; n++) {
                if (CheckCollisionPointRec(mousePos, navRects[n])) mode = navItems[n].mode;
            }
            if (CheckCollisionPointRec(mousePos, themeBtn)) {
                g_theme = (g_theme == THEME_DARK) ? THEME_LIGHT : THEME_DARK;
            }
            if (CheckCollisionPointRec(mousePos, fontBtn)) {
                ReloadFont(g_fontChoiceIndex + 1);
            }
        }

        float contentX = sidebarWidth;
        float contentWidth = currentWidth - sidebarWidth;

        BeginDrawing();
            ClearBackground(pal.bg);

            // ---------------- Sidebar ----------------
            DrawRectangleRec(sidebar, pal.panelDark);
            DrawLine((int)sidebarWidth, 0, (int)sidebarWidth, (int)currentHeight, pal.panel);

            int navFontSize = (int)(14 * scale);
            for (int n = 0; n < navCount; n++) {
                bool active = (mode == navItems[n].mode);
                bool hover = CheckCollisionPointRec(mousePos, navRects[n]);
                Color col = active ? pal.accent : (hover ? pal.hover : pal.panel);
                DrawRectangleRec(navRects[n], col);

                Color labelColor = active ? pal.textOnAccent : pal.textPrimary;
                if (navItems[n].line2) {
                    int lineH = navFontSize + 2;
                    int w1 = MeasureText(navItems[n].line1, navFontSize);
                    int w2 = MeasureText(navItems[n].line2, navFontSize);
                    float blockH = (float)(lineH * 2);
                    float startY = navRects[n].y + (navRects[n].height - blockH) / 2;
                    DrawText(navItems[n].line1, (int)(navRects[n].x + (navRects[n].width - w1) / 2), (int)startY, navFontSize, labelColor);
                    DrawText(navItems[n].line2, (int)(navRects[n].x + (navRects[n].width - w2) / 2), (int)(startY + lineH), navFontSize, labelColor);
                } else {
                    int w = MeasureText(navItems[n].line1, navFontSize);
                    DrawText(navItems[n].line1, (int)(navRects[n].x + (navRects[n].width - w) / 2), (int)(navRects[n].y + (navRects[n].height - navFontSize) / 2), navFontSize, labelColor);
                }
            }

            // Footer: font cycle + theme toggle
            {
                int footerFontSize = (int)(13 * scale);
                bool hoverFont = CheckCollisionPointRec(mousePos, fontBtn);
                bool hoverTheme = CheckCollisionPointRec(mousePos, themeBtn);

                DrawRectangleRec(fontBtn, hoverFont ? pal.hover : pal.panel);
                std::string fontLabel = std::string("Font: ") + g_fontChoices[g_fontChoiceIndex].label;
                // Shrink to a shorter label if it doesn't fit at this scale/window size.
                if (MeasureText(fontLabel.c_str(), footerFontSize) > fontBtn.width - 12 * scale) {
                    fontLabel = "Font: change";
                }
                int fw = MeasureText(fontLabel.c_str(), footerFontSize);
                DrawText(fontLabel.c_str(), (int)(fontBtn.x + (fontBtn.width - fw) / 2), (int)(fontBtn.y + (fontBtn.height - footerFontSize) / 2), footerFontSize, pal.textPrimary);

                DrawRectangleRec(themeBtn, hoverTheme ? pal.hover : pal.panel);
                const char* themeLabel = (g_theme == THEME_DARK) ? "Switch to Light" : "Switch to Dark";
                int tw = MeasureText(themeLabel, footerFontSize);
                DrawText(themeLabel, (int)(themeBtn.x + (themeBtn.width - tw) / 2), (int)(themeBtn.y + (themeBtn.height - footerFontSize) / 2), footerFontSize, pal.textPrimary);
            }

            // ================================================================
            // MODE: WORD COUNTER
            // ================================================================
            if (mode == MODE_WORD_COUNTER) {
                int titleFontSize = (int)(30 * scale);
                int subTitleFontSize = (int)(14 * scale);
                int inputFontSize = (int)(22 * scale);
                int btnFontSize = (int)(17 * scale);
                int statsFontSize = (int)(22 * scale);

                Rectangle inputBox = { contentX + 40 * scale, 110 * scale, contentWidth - 80 * scale, currentHeight - 320 * scale };

                float btnW = 140 * scale;
                float btnH = 45 * scale;
                Rectangle saveBtn = { contentX + 40 * scale, currentHeight - 190 * scale, btnW, btnH };
                Rectangle loadBtn = { contentX + 40 * scale + btnW + 20 * scale, currentHeight - 190 * scale, btnW, btnH };

                Rectangle dashboard = { contentX + 40 * scale, currentHeight - 120 * scale, contentWidth - 80 * scale, 70 * scale };

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(mousePos, inputBox)) {
                        boxSelected = true;
                        if (textInput == "Type or load text here...") textInput = "";
                    } else if (mousePos.x > contentX) {
                        boxSelected = false;
                        isAllSelected = false;
                    }
                }

                if (boxSelected) {
                    bool ctrlPressed = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
                    if (ctrlPressed && IsKeyPressed(KEY_A)) isAllSelected = true;

                    int key = GetCharPressed();
                    if (key > 0 && !ctrlPressed) {
                        if (isAllSelected) { textInput = ""; isAllSelected = false; }
                        if ((key >= 32) && (key <= 125) && textInput.length() < 3000) textInput += (char)key;
                    }
                    if (IsKeyPressed(KEY_SPACE) && !ctrlPressed && textInput.length() < 3000) {
                        if (isAllSelected) { textInput = ""; isAllSelected = false; }
                    }
                    if (IsKeyPressed(KEY_BACKSPACE)) {
                        if (isAllSelected) { textInput = ""; isAllSelected = false; }
                        else if (!textInput.empty()) textInput.pop_back();
                    }
                }

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(mousePos, saveBtn)) {
                        std::string selectedPath = SaveFileDialog();
                        if (!selectedPath.empty()) {
                            std::ofstream outFile(selectedPath);
                            if (outFile.is_open()) { outFile << textInput; outFile.close(); }
                        }
                    }
                    if (CheckCollisionPointRec(mousePos, loadBtn)) {
                        std::string selectedPath = OpenFileDialog();
                        if (!selectedPath.empty()) {
                            std::ifstream inFile(selectedPath);
                            if (inFile.is_open()) {
                                std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
                                textInput = content;
                                inFile.close();
                                boxSelected = true;
                                isAllSelected = false;
                            }
                        }
                    }
                }

                int charCount = (int)textInput.length();
                int wordCount = CountWords(textInput);
                std::vector<std::string> wrappedLines = WrapText(textInput, inputFontSize, (int)(inputBox.width - 30 * scale));
                int lineSpacing = (int)(inputFontSize * 1.4f);

                DrawText("Word Counter", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, pal.accent);
                DrawText("Press F11 to Toggle Fullscreen | Ctrl+A: Select All", (int)(contentX + 40 * scale), (int)(70 * scale), subTitleFontSize, pal.textSecondary);

                DrawRectangleRec(inputBox, pal.panel);
                DrawRectangleLinesEx(inputBox, 2, boxSelected ? pal.danger : pal.borderIdle);

                float padX = 15 * scale;
                float padY = 15 * scale;

                if (textInput.empty() && !boxSelected) {
                    DrawText("Type or load text here...", (int)(inputBox.x + padX), (int)(inputBox.y + padY), inputFontSize, pal.textSecondary);
                } else if (isAllSelected && !textInput.empty()) {
                    int lineY = (int)(inputBox.y + padY);
                    for (const auto& line : wrappedLines) {
                        if (lineY + inputFontSize < inputBox.y + inputBox.height) {
                            int textW = MeasureText(line.c_str(), inputFontSize);
                            DrawRectangle((int)(inputBox.x + padX), lineY, textW, inputFontSize + 4, pal.selectionHighlight);
                            DrawText(line.c_str(), (int)(inputBox.x + padX), lineY, inputFontSize, pal.textPrimary);
                        }
                        lineY += lineSpacing;
                    }
                } else {
                    int lineY = (int)(inputBox.y + padY);
                    for (const auto& line : wrappedLines) {
                        if (lineY + inputFontSize < inputBox.y + inputBox.height) {
                            DrawText(line.c_str(), (int)(inputBox.x + padX), lineY, inputFontSize, pal.textPrimary);
                        }
                        lineY += lineSpacing;
                    }
                    if (boxSelected && ((GetTime() * 2) - (int)(GetTime() * 2) > 0.5)) {
                        int lastLineW = wrappedLines.empty() ? 0 : MeasureText(wrappedLines.back().c_str(), inputFontSize);
                        int cursorY = wrappedLines.empty() ? (int)(inputBox.y + padY) : (int)(inputBox.y + padY + (wrappedLines.size() - 1) * lineSpacing);
                        if (cursorY < inputBox.y + inputBox.height - lineSpacing) {
                            DrawRectangle((int)(inputBox.x + padX + lastLineW), cursorY, 2, inputFontSize, pal.danger);
                        }
                    }
                }

                bool hoverSave = CheckCollisionPointRec(mousePos, saveBtn);
                bool hoverLoad = CheckCollisionPointRec(mousePos, loadBtn);
                DrawRectangleRec(saveBtn, hoverSave ? pal.hoverLight : pal.textSecondary);
                int saveTxtW = MeasureText("Save As...", btnFontSize);
                DrawText("Save As...", (int)(saveBtn.x + (saveBtn.width - saveTxtW) / 2), (int)(saveBtn.y + (saveBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);
                DrawRectangleRec(loadBtn, hoverLoad ? pal.hoverLight : pal.textSecondary);
                int loadTxtW = MeasureText("Open File", btnFontSize);
                DrawText("Open File", (int)(loadBtn.x + (loadBtn.width - loadTxtW) / 2), (int)(loadBtn.y + (loadBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);

                DrawRectangleRec(dashboard, pal.panelDark);
                std::string wordsStr = "Words: " + std::to_string(wordCount);
                std::string charsStr = "Characters: " + std::to_string(charCount);
                DrawText(wordsStr.c_str(), (int)(dashboard.x + 40 * scale), (int)(dashboard.y + (dashboard.height - statsFontSize) / 2), statsFontSize, pal.success);
                DrawText(charsStr.c_str(), (int)(dashboard.x + dashboard.width / 2), (int)(dashboard.y + (dashboard.height - statsFontSize) / 2), statsFontSize, pal.warning);
            }

            // ================================================================
            // MODE: CALCULATOR
            // ================================================================
            else if (mode == MODE_CALCULATOR) {
                int titleFontSize = (int)(30 * scale);
                int displayFontSize = (int)(34 * scale);
                int exprFontSize = (int)(16 * scale);
                int keyFontSize = (int)(22 * scale);

                DrawText("Calculator", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, pal.accent);

                float panelW = 360 * scale;
                if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                float panelX = contentX + (contentWidth - panelW) / 2;
                float panelY = 90 * scale;

                Rectangle displayBox = { panelX, panelY, panelW, 90 * scale };
                DrawRectangleRec(displayBox, pal.panel);
                DrawRectangleLinesEx(displayBox, 2, pal.borderIdle);

                int exprW = MeasureText(calcExpr.c_str(), exprFontSize);
                DrawText(calcExpr.c_str(), (int)(displayBox.x + displayBox.width - 15 * scale - exprW), (int)(displayBox.y + 10 * scale), exprFontSize, pal.textSecondary);

                Color displayColor = calcError ? pal.danger : pal.textPrimary;
                int dispW = MeasureText(calcDisplay.c_str(), displayFontSize);
                DrawText(calcDisplay.c_str(), (int)(displayBox.x + displayBox.width - 15 * scale - dispW), (int)(displayBox.y + displayBox.height - displayFontSize - 10 * scale), displayFontSize, displayColor);

                const char* keys[] = {
                    "(", ")", "C", "/",
                    "7", "8", "9", "*",
                    "4", "5", "6", "-",
                    "1", "2", "3", "+",
                    "0", ".", "=", "="
                };

                int cols = 4, rows = 5;
                float gap = 8 * scale;
                float keyW = (panelW - gap * (cols - 1)) / cols;
                float keyH = 55 * scale;
                float gridY = displayBox.y + displayBox.height + 15 * scale;

                for (int r = 0; r < rows; r++) {
                    for (int c = 0; c < cols; c++) {
                        int idx = r * cols + c;
                        if (r == 4 && c == 3) continue;

                        float bx = panelX + c * (keyW + gap);
                        float by = gridY + r * (keyH + gap);
                        float bw = keyW;
                        if (r == 4 && c == 2) bw = keyW * 2 + gap;

                        Rectangle btn = { bx, by, bw, keyH };
                        const char* label = keys[idx];

                        bool isOp = (label[0] == '/' || label[0] == '*' || label[0] == '-' || label[0] == '+');
                        bool isEquals = (label[0] == '=');
                        bool isClear = (label[0] == 'C');

                        bool hover = CheckCollisionPointRec(mousePos, btn);
                        Color base = isEquals ? pal.accent : (isClear ? pal.dangerDim : (isOp ? pal.hover : pal.panel));
                        Color col = hover ? ColorBrightness(base, 0.25f) : base;

                        DrawRectangleRec(btn, col);
                        int lw = MeasureText(label, keyFontSize);
                        DrawText(label, (int)(btn.x + (btn.width - lw) / 2), (int)(btn.y + (btn.height - keyFontSize) / 2), keyFontSize, pal.textPrimary);

                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover) {
                            if (isClear) {
                                calcExpr = ""; calcDisplay = "0"; calcError = false;
                            } else if (isEquals) {
                                double result = 0.0;
                                if (!calcExpr.empty() && EvaluateExpression(calcExpr, result)) { calcDisplay = FormatNumber(result); calcError = false; }
                                else if (!calcExpr.empty()) { calcDisplay = "Error"; calcError = true; }
                            } else if (calcExpr.length() < 64) {
                                calcExpr += label; calcError = false;
                            }
                        }
                    }
                }

                {
                    int key = GetCharPressed();
                    while (key > 0) {
                        bool valid = (key >= '0' && key <= '9') || key == '.' || key == '+' || key == '-' || key == '*' || key == '/' || key == '(' || key == ')';
                        if (valid && calcExpr.length() < 64) { calcExpr += (char)key; calcError = false; }
                        key = GetCharPressed();
                    }
                    if (IsKeyPressed(KEY_BACKSPACE) && !calcExpr.empty()) { calcExpr.pop_back(); calcError = false; }
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                        double result = 0.0;
                        if (!calcExpr.empty() && EvaluateExpression(calcExpr, result)) { calcDisplay = FormatNumber(result); calcError = false; }
                        else if (!calcExpr.empty()) { calcDisplay = "Error"; calcError = true; }
                    }
                    if (IsKeyPressed(KEY_ESCAPE)) { calcExpr = ""; calcDisplay = "0"; calcError = false; }
                }
            }

            // ================================================================
            // MODE: TIME (Countdown / Stopwatch / Alarm / World Clock)
            // ================================================================
            else if (mode == MODE_TIME) {
                int titleFontSize = (int)(28 * scale);
                int tabFontSize = (int)(14 * scale);
                int labelFontSize = (int)(15 * scale);
                int bigFontSize = (int)(58 * scale);
                int btnFontSize = (int)(16 * scale);

                DrawText("Time", (int)(contentX + 40 * scale), (int)(25 * scale), titleFontSize, pal.accent);

                // Sub-tab bar
                const char* subTabs[] = { "Countdown", "Stopwatch", "Alarm", "World Clock" };
                int subTabCount = 4;
                float subTabY = 65 * scale;
                float subTabGap = 8 * scale;
                float subTabAreaW = contentWidth - 80 * scale;
                float subTabW = (subTabAreaW - subTabGap * (subTabCount - 1)) / subTabCount;
                float subTabH = 36 * scale;

                for (int i = 0; i < subTabCount; i++) {
                    Rectangle tabRect = { contentX + 40 * scale + i * (subTabW + subTabGap), subTabY, subTabW, subTabH };
                    bool active = ((int)timeSubMode == i);
                    bool hover = CheckCollisionPointRec(mousePos, tabRect);
                    DrawRectangleRec(tabRect, active ? pal.accent : (hover ? pal.hover : pal.panel));
                    Color labelCol = active ? pal.textOnAccent : pal.textPrimary;
                    int tw = MeasureText(subTabs[i], tabFontSize);
                    DrawText(subTabs[i], (int)(tabRect.x + (tabRect.width - tw) / 2), (int)(tabRect.y + (tabRect.height - tabFontSize) / 2), tabFontSize, labelCol);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover) timeSubMode = (TimeSubMode)i;
                }

                float panelTop = subTabY + subTabH + 25 * scale;

                // ---------------- Countdown ----------------
                if (timeSubMode == TIME_COUNTDOWN) {
                    float panelW = 420 * scale;
                    if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                    float panelX = contentX + (contentWidth - panelW) / 2;

                    int totalSecs = (int)(timerRemaining + 0.5);
                    int mm = totalSecs / 60;
                    int ss = totalSecs % 60;
                    char timeBuf[16];
                    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mm, ss);

                    bool finished = (timerRemaining <= 0.0) && !timerRunning && timerTotalSeconds > 0.0;
                    bool blink = finished && (((GetTime() * 2) - (int)(GetTime() * 2)) > 0.5);
                    Color timeColor = finished ? (blink ? pal.danger : pal.dangerDim) : pal.textPrimary;

                    int timeW = MeasureText(timeBuf, bigFontSize);
                    DrawText(timeBuf, (int)(panelX + (panelW - timeW) / 2), (int)panelTop, bigFontSize, timeColor);

                    if (finished) {
                        const char* msg = "Time's up!";
                        int msgW = MeasureText(msg, labelFontSize);
                        DrawText(msg, (int)(panelX + (panelW - msgW) / 2), (int)(panelTop + bigFontSize + 10 * scale), labelFontSize, pal.danger);
                    }

                    const int presets[] = { 1, 5, 10, 15, 30 };
                    int presetCount = 5;
                    float presetY = panelTop + bigFontSize + 45 * scale;
                    float presetGap = 10 * scale;
                    float presetW = (panelW - presetGap * (presetCount - 1)) / presetCount;
                    float presetH = 40 * scale;

                    for (int p = 0; p < presetCount; p++) {
                        Rectangle btn = { panelX + p * (presetW + presetGap), presetY, presetW, presetH };
                        bool hover = CheckCollisionPointRec(mousePos, btn);
                        DrawRectangleRec(btn, hover ? pal.hover : pal.panel);
                        std::string label = std::to_string(presets[p]) + "m";
                        int lw = MeasureText(label.c_str(), btnFontSize);
                        DrawText(label.c_str(), (int)(btn.x + (btn.width - lw) / 2), (int)(btn.y + (btn.height - btnFontSize) / 2), btnFontSize, pal.textPrimary);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover) {
                            timerTotalSeconds = presets[p] * 60.0;
                            timerRemaining = timerTotalSeconds;
                            timerRunning = false;
                            timerMinutesInput = std::to_string(presets[p]);
                        }
                    }

                    float fieldY = presetY + presetH + 25 * scale;
                    Rectangle fieldBox = { panelX, fieldY, 120 * scale, 45 * scale };
                    Rectangle setBtn = { panelX + 130 * scale, fieldY, 100 * scale, 45 * scale };

                    bool hoverField = CheckCollisionPointRec(mousePos, fieldBox);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) timerFieldActive = hoverField;

                    DrawRectangleRec(fieldBox, pal.panel);
                    DrawRectangleLinesEx(fieldBox, 2, timerFieldActive ? pal.danger : pal.borderIdle);
                    std::string fieldDisp = timerMinutesInput.empty() ? "0" : timerMinutesInput;
                    DrawText(fieldDisp.c_str(), (int)(fieldBox.x + 12 * scale), (int)(fieldBox.y + (fieldBox.height - btnFontSize) / 2), btnFontSize, pal.textPrimary);
                    DrawText("min", (int)(fieldBox.x + fieldBox.width + 8 * scale), (int)(fieldBox.y + (fieldBox.height - labelFontSize) / 2), labelFontSize, pal.textSecondary);

                    if (timerFieldActive) {
                        int key = GetCharPressed();
                        while (key > 0) {
                            if (key >= '0' && key <= '9' && timerMinutesInput.length() < 4) timerMinutesInput += (char)key;
                            key = GetCharPressed();
                        }
                        if (IsKeyPressed(KEY_BACKSPACE) && !timerMinutesInput.empty()) timerMinutesInput.pop_back();
                    }

                    bool hoverSet = CheckCollisionPointRec(mousePos, setBtn);
                    DrawRectangleRec(setBtn, hoverSet ? pal.hoverLight : pal.textSecondary);
                    int setW = MeasureText("Set", btnFontSize);
                    DrawText("Set", (int)(setBtn.x + (setBtn.width - setW) / 2), (int)(setBtn.y + (setBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverSet && !timerMinutesInput.empty()) {
                        int mins = atoi(timerMinutesInput.c_str());
                        timerTotalSeconds = mins * 60.0;
                        timerRemaining = timerTotalSeconds;
                        timerRunning = false;
                    }

                    float ctrlY = fieldY + 45 * scale + 25 * scale;
                    Rectangle startBtn = { panelX, ctrlY, panelW / 2 - 10 * scale, 50 * scale };
                    Rectangle resetBtn = { panelX + panelW / 2 + 10 * scale, ctrlY, panelW / 2 - 10 * scale, 50 * scale };

                    bool hoverStart = CheckCollisionPointRec(mousePos, startBtn);
                    bool hoverReset = CheckCollisionPointRec(mousePos, resetBtn);

                    DrawRectangleRec(startBtn, timerRunning ? pal.dangerDim : (hoverStart ? ColorBrightness(pal.successDim, 0.3f) : pal.successDim));
                    const char* startLabel = timerRunning ? "Pause" : "Start";
                    int startW = MeasureText(startLabel, btnFontSize);
                    DrawText(startLabel, (int)(startBtn.x + (startBtn.width - startW) / 2), (int)(startBtn.y + (startBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);

                    DrawRectangleRec(resetBtn, hoverReset ? pal.hoverLight : pal.textSecondary);
                    int resetW = MeasureText("Reset", btnFontSize);
                    DrawText("Reset", (int)(resetBtn.x + (resetBtn.width - resetW) / 2), (int)(resetBtn.y + (resetBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (hoverStart && timerRemaining > 0.0) timerRunning = !timerRunning;
                        if (hoverReset) { timerRemaining = timerTotalSeconds; timerRunning = false; }
                    }
                }

                // ---------------- Stopwatch ----------------
                else if (timeSubMode == TIME_STOPWATCH) {
                    float panelW = 420 * scale;
                    if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                    float panelX = contentX + (contentWidth - panelW) / 2;

                    int totalMs = (int)(stopwatchElapsed * 1000.0);
                    int mm = (totalMs / 60000);
                    int ss = (totalMs / 1000) % 60;
                    int ms = (totalMs % 1000) / 10; // centiseconds
                    char timeBuf[24];
                    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d.%02d", mm, ss, ms);

                    int timeW = MeasureText(timeBuf, bigFontSize);
                    DrawText(timeBuf, (int)(panelX + (panelW - timeW) / 2), (int)panelTop, bigFontSize, pal.textPrimary);

                    float ctrlY = panelTop + bigFontSize + 30 * scale;
                    Rectangle startBtn = { panelX, ctrlY, panelW / 2 - 10 * scale, 50 * scale };
                    Rectangle lapResetBtn = { panelX + panelW / 2 + 10 * scale, ctrlY, panelW / 2 - 10 * scale, 50 * scale };

                    bool hoverStart = CheckCollisionPointRec(mousePos, startBtn);
                    bool hoverLapReset = CheckCollisionPointRec(mousePos, lapResetBtn);

                    DrawRectangleRec(startBtn, stopwatchRunning ? pal.dangerDim : (hoverStart ? ColorBrightness(pal.successDim, 0.3f) : pal.successDim));
                    const char* startLabel = stopwatchRunning ? "Stop" : "Start";
                    int startW = MeasureText(startLabel, btnFontSize);
                    DrawText(startLabel, (int)(startBtn.x + (startBtn.width - startW) / 2), (int)(startBtn.y + (startBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);

                    // "Lap" while running, "Reset" while stopped -- same button slot.
                    const char* lapResetLabel = stopwatchRunning ? "Lap" : "Reset";
                    DrawRectangleRec(lapResetBtn, hoverLapReset ? pal.hoverLight : pal.textSecondary);
                    int lrW = MeasureText(lapResetLabel, btnFontSize);
                    DrawText(lapResetLabel, (int)(lapResetBtn.x + (lapResetBtn.width - lrW) / 2), (int)(lapResetBtn.y + (lapResetBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (hoverStart) stopwatchRunning = !stopwatchRunning;
                        if (hoverLapReset) {
                            if (stopwatchRunning) {
                                if (stopwatchLaps.size() < 20) stopwatchLaps.push_back(stopwatchElapsed);
                            } else {
                                stopwatchElapsed = 0.0;
                                stopwatchLaps.clear();
                            }
                        }
                    }

                    // Lap list
                    float lapY = ctrlY + 50 * scale + 20 * scale;
                    int lapFontSize = (int)(14 * scale);
                    for (int i = (int)stopwatchLaps.size() - 1; i >= 0 && lapY < currentHeight - 20 * scale; i--) {
                        int lapMs = (int)(stopwatchLaps[i] * 1000.0);
                        int lmm = lapMs / 60000, lss = (lapMs / 1000) % 60, lms = (lapMs % 1000) / 10;
                        char lapBuf[48];
                        snprintf(lapBuf, sizeof(lapBuf), "Lap %d: %02d:%02d.%02d", i + 1, lmm, lss, lms);
                        DrawText(lapBuf, (int)panelX, (int)lapY, lapFontSize, pal.textSecondary);
                        lapY += lapFontSize + 8 * scale;
                    }
                }

                // ---------------- Alarm ----------------
                else if (timeSubMode == TIME_ALARM) {
                    float panelW = 420 * scale;
                    if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                    float panelX = contentX + (contentWidth - panelW) / 2;

                    // HH : MM fields
                    Rectangle hourBox = { panelX, panelTop, 90 * scale, 60 * scale };
                    Rectangle minuteBox = { panelX + 110 * scale, panelTop, 90 * scale, 60 * scale };

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        alarmHourFieldActive = CheckCollisionPointRec(mousePos, hourBox);
                        alarmMinuteFieldActive = CheckCollisionPointRec(mousePos, minuteBox);
                    }

                    DrawRectangleRec(hourBox, pal.panel);
                    DrawRectangleLinesEx(hourBox, 2, alarmHourFieldActive ? pal.danger : pal.borderIdle);
                    int hw = MeasureText(alarmHourInput.c_str(), bigFontSize / 2);
                    DrawText(alarmHourInput.c_str(), (int)(hourBox.x + (hourBox.width - hw) / 2), (int)(hourBox.y + (hourBox.height - bigFontSize / 2) / 2), bigFontSize / 2, pal.textPrimary);

                    int colonW = MeasureText(":", bigFontSize / 2);
                    DrawText(":", (int)(hourBox.x + hourBox.width + (110 * scale - hourBox.width - colonW) / 2), (int)(hourBox.y + (hourBox.height - bigFontSize / 2) / 2), bigFontSize / 2, pal.textPrimary);

                    DrawRectangleRec(minuteBox, pal.panel);
                    DrawRectangleLinesEx(minuteBox, 2, alarmMinuteFieldActive ? pal.danger : pal.borderIdle);
                    int mw = MeasureText(alarmMinuteInput.c_str(), bigFontSize / 2);
                    DrawText(alarmMinuteInput.c_str(), (int)(minuteBox.x + (minuteBox.width - mw) / 2), (int)(minuteBox.y + (minuteBox.height - bigFontSize / 2) / 2), bigFontSize / 2, pal.textPrimary);

                    if (alarmHourFieldActive) {
                        int key = GetCharPressed();
                        while (key > 0) {
                            if (key >= '0' && key <= '9' && alarmHourInput.length() < 2) alarmHourInput += (char)key;
                            key = GetCharPressed();
                        }
                        if (IsKeyPressed(KEY_BACKSPACE) && !alarmHourInput.empty()) alarmHourInput.pop_back();
                    }
                    if (alarmMinuteFieldActive) {
                        int key = GetCharPressed();
                        while (key > 0) {
                            if (key >= '0' && key <= '9' && alarmMinuteInput.length() < 2) alarmMinuteInput += (char)key;
                            key = GetCharPressed();
                        }
                        if (IsKeyPressed(KEY_BACKSPACE) && !alarmMinuteInput.empty()) alarmMinuteInput.pop_back();
                    }

                    float applyY = panelTop + 60 * scale + 20 * scale;
                    Rectangle applyBtn = { panelX, applyY, 150 * scale, 45 * scale };
                    bool hoverApply = CheckCollisionPointRec(mousePos, applyBtn);
                    DrawRectangleRec(applyBtn, hoverApply ? pal.hoverLight : pal.textSecondary);
                    int appW = MeasureText("Apply Time", btnFontSize);
                    DrawText("Apply Time", (int)(applyBtn.x + (applyBtn.width - appW) / 2), (int)(applyBtn.y + (applyBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverApply) {
                        int h = alarmHourInput.empty() ? 0 : atoi(alarmHourInput.c_str());
                        int m = alarmMinuteInput.empty() ? 0 : atoi(alarmMinuteInput.c_str());
                        if (h < 0) h = 0; if (h > 23) h = 23;
                        if (m < 0) m = 0; if (m > 59) m = 59;
                        alarmHour = h; alarmMinute = m;
                        char buf[4];
                        snprintf(buf, sizeof(buf), "%02d", h); alarmHourInput = buf;
                        snprintf(buf, sizeof(buf), "%02d", m); alarmMinuteInput = buf;
                        alarmLastTriggeredKey = -1; // allow it to fire again at the new time
                    }

                    float toggleY = applyY + 45 * scale + 20 * scale;
                    Rectangle toggleBtn = { panelX, toggleY, 200 * scale, 50 * scale };
                    bool hoverToggle = CheckCollisionPointRec(mousePos, toggleBtn);
                    DrawRectangleRec(toggleBtn, alarmEnabled ? pal.successDim : (hoverToggle ? pal.hover : pal.panel));
                    char toggleBuf[64];
                    snprintf(toggleBuf, sizeof(toggleBuf), "Alarm: %s (%02d:%02d)", alarmEnabled ? "ON" : "OFF", alarmHour, alarmMinute);
                    int togW = MeasureText(toggleBuf, btnFontSize);
                    DrawText(toggleBuf, (int)(toggleBtn.x + (toggleBtn.width - togW) / 2), (int)(toggleBtn.y + (toggleBtn.height - btnFontSize) / 2), btnFontSize, pal.textPrimary);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverToggle) {
                        alarmEnabled = !alarmEnabled;
                        if (alarmEnabled) alarmLastTriggeredKey = -1;
                        alarmRinging = false;
                    }

                    if (alarmRinging) {
                        float ringY = toggleY + 50 * scale + 30 * scale;
                        bool blink = (((GetTime() * 2) - (int)(GetTime() * 2)) > 0.5);
                        const char* msg = "ALARM! Wake up!";
                        int mwid = MeasureText(msg, bigFontSize / 2);
                        DrawText(msg, (int)(panelX + (panelW - mwid) / 2), (int)ringY, bigFontSize / 2, blink ? pal.danger : pal.dangerDim);

                        Rectangle dismissBtn = { panelX + (panelW - 160 * scale) / 2, ringY + bigFontSize / 2 + 20 * scale, 160 * scale, 45 * scale };
                        bool hoverDismiss = CheckCollisionPointRec(mousePos, dismissBtn);
                        DrawRectangleRec(dismissBtn, hoverDismiss ? ColorBrightness(pal.dangerDim, 0.3f) : pal.dangerDim);
                        int dw = MeasureText("Dismiss", btnFontSize);
                        DrawText("Dismiss", (int)(dismissBtn.x + (dismissBtn.width - dw) / 2), (int)(dismissBtn.y + (dismissBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverDismiss) alarmRinging = false;
                    }
                }

                // ---------------- World Clock ----------------
                else if (timeSubMode == TIME_WORLDCLOCK) {
                    time_t nowT = time(nullptr);

                    float listX = contentX + 40 * scale;
                    float listW = contentWidth - 80 * scale;
                    float rowH = 60 * scale;
                    float rowGap = 10 * scale;
                    float y = panelTop;

                    int cardFontSize = (int)(15 * scale);
                    int cardTimeFontSize = (int)(26 * scale);

                    for (size_t i = 0; i < worldClocks.size(); i++) {
                        WorldClockEntry& entry = worldClocks[i];
                        Rectangle row = { listX, y, listW, rowH };
                        DrawRectangleRec(row, pal.panel);

                        struct tm displayTm;
                        if (entry.isLocal) {
                            displayTm = *localtime(&nowT);
                        } else {
                            time_t adjusted = nowT + (time_t)(entry.offsetHours * 3600.0);
                            displayTm = *gmtime(&adjusted);
                        }

                        std::string timeStr = FormatClockTime(displayTm);
                        std::string dateStr = FormatClockDate(displayTm);

                        DrawText(entry.displayName.c_str(), (int)(row.x + 20 * scale), (int)(row.y + 8 * scale), cardFontSize, pal.textPrimary);
                        std::string subLabel = dateStr + (entry.note.empty() ? "" : ("  ·  " + entry.note));
                        DrawText(subLabel.c_str(), (int)(row.x + 20 * scale), (int)(row.y + 8 * scale + cardFontSize + 4 * scale), (int)(cardFontSize * 0.8f), pal.textSecondary);

                        int timeW = MeasureText(timeStr.c_str(), cardTimeFontSize);
                        DrawText(timeStr.c_str(), (int)(row.x + row.width - 20 * scale - timeW), (int)(row.y + (row.height - cardTimeFontSize) / 2), cardTimeFontSize, pal.success);

                        // Remove button for anything past the two built-in entries (Local, UTC).
                        if (i >= 2) {
                            Rectangle removeBtn = { row.x + row.width - 20 * scale - timeW - 40 * scale, row.y + (row.height - 28 * scale) / 2, 28 * scale, 28 * scale };
                            bool hoverRemove = CheckCollisionPointRec(mousePos, removeBtn);
                            DrawRectangleRec(removeBtn, hoverRemove ? pal.dangerDim : pal.panelDark);
                            int xw = MeasureText("x", cardFontSize);
                            DrawText("x", (int)(removeBtn.x + (removeBtn.width - xw) / 2), (int)(removeBtn.y + (removeBtn.height - cardFontSize) / 2), cardFontSize, pal.textPrimary);
                            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverRemove) {
                                worldClocks.erase(worldClocks.begin() + i);
                                y -= (rowH + rowGap); // stay visually consistent this frame
                            }
                        }

                        y += rowH + rowGap;
                    }

                    // Add-a-city search field
                    y += 10 * scale;
                    Rectangle searchBox = { listX, y, listW - 110 * scale, 50 * scale };
                    Rectangle addBtn = { listX + listW - 100 * scale, y, 100 * scale, 50 * scale };

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) worldClockFieldActive = CheckCollisionPointRec(mousePos, searchBox);

                    DrawRectangleRec(searchBox, pal.panel);
                    DrawRectangleLinesEx(searchBox, 2, worldClockFieldActive ? pal.danger : pal.borderIdle);
                    std::string searchDisp = worldClockSearch.empty() ? "Add a city, e.g. \"Beijing\"..." : worldClockSearch;
                    Color searchCol = worldClockSearch.empty() ? pal.textSecondary : pal.textPrimary;
                    DrawText(searchDisp.c_str(), (int)(searchBox.x + 15 * scale), (int)(searchBox.y + (searchBox.height - cardFontSize) / 2), cardFontSize, searchCol);

                    bool submit = false;
                    if (worldClockFieldActive) {
                        int key = GetCharPressed();
                        while (key > 0) {
                            if (key >= 32 && key <= 125 && worldClockSearch.length() < 60) worldClockSearch += (char)key;
                            key = GetCharPressed();
                        }
                        if (IsKeyPressed(KEY_BACKSPACE) && !worldClockSearch.empty()) worldClockSearch.pop_back();
                        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) submit = true;
                    }

                    bool hoverAdd = CheckCollisionPointRec(mousePos, addBtn);
                    DrawRectangleRec(addBtn, hoverAdd ? pal.hoverLight : pal.textSecondary);
                    int addW = MeasureText("Add", btnFontSize);
                    DrawText("Add", (int)(addBtn.x + (addBtn.width - addW) / 2), (int)(addBtn.y + (addBtn.height - btnFontSize) / 2), btnFontSize, pal.textOnAccent);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverAdd) submit = true;

                    if (submit && !worldClockSearch.empty()) {
                        std::string query = ToLowerCopy(TrimCopy(worldClockSearch));
                        const CityTimezone* match = nullptr;
                        for (auto& ct : g_cityTimezones) if (ct.city == query) { match = &ct; break; }

                        if (!match) {
                            worldClockError = "City not recognized. Try a major city name (see README for the list), or add an entry to g_cityTimezones in main.cpp.";
                        } else {
                            double offset = match->approxOffsetHours;
                            std::string note = "offline estimate, DST not applied";
                            double liveOffset = 0.0;
                            // This blocks briefly while it makes a real network request.
                            if (FetchUtcOffsetForTimezone(match->ianaTz, liveOffset)) {
                                offset = liveOffset;
                                note = "live";
                            }
                            std::string displayName = TrimCopy(worldClockSearch);
                            if (!displayName.empty()) displayName[0] = (char)toupper((unsigned char)displayName[0]);
                            worldClocks.push_back({ displayName, false, offset, note });
                            worldClockError = "";
                        }
                        worldClockSearch = "";
                    }

                    if (!worldClockError.empty()) {
                        y += 50 * scale + 15 * scale;
                        std::vector<std::string> errLines = WrapText(worldClockError, (int)(12 * scale), (int)listW);
                        for (auto& line : errLines) {
                            DrawText(line.c_str(), (int)listX, (int)y, (int)(12 * scale), pal.danger);
                            y += (int)(12 * scale) + 4 * scale;
                        }
                    }
                }
            }

            // ================================================================
            // MODE: UNIT / CURRENCY CONVERTER
            // ================================================================
            else if (mode == MODE_CONVERTER) {
                int titleFontSize = (int)(30 * scale);
                int labelFontSize = (int)(15 * scale);
                int fieldFontSize = (int)(24 * scale);
                int tabFontSize = (int)(14 * scale);

                DrawText("Unit & Currency Converter", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, pal.accent);

                float panelW = 460 * scale;
                if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                float panelX = contentX + (contentWidth - panelW) / 2;
                float panelY = 90 * scale;

                int catCount = (int)g_categories.size();
                float tabGap = 8 * scale;
                float tabW = (panelW - tabGap * (catCount - 1)) / catCount;
                float tabH = 38 * scale;

                for (int i = 0; i < catCount; i++) {
                    Rectangle tabBtn = { panelX + i * (tabW + tabGap), panelY, tabW, tabH };
                    bool active = (i == convCategory);
                    bool hover = CheckCollisionPointRec(mousePos, tabBtn);
                    DrawRectangleRec(tabBtn, active ? pal.accent : (hover ? pal.hover : pal.panel));

                    std::string label = g_categories[i].name;
                    Color labelCol = active ? pal.textOnAccent : pal.textPrimary;
                    int lw = MeasureText(label.c_str(), tabFontSize);
                    DrawText(label.c_str(), (int)(tabBtn.x + (tabBtn.width - lw) / 2), (int)(tabBtn.y + (tabBtn.height - tabFontSize) / 2), tabFontSize, labelCol);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover && !active) {
                        convCategory = i;
                        convFromUnit = 0;
                        convToUnit = (int)g_categories[i].units.size() > 1 ? 1 : 0;
                    }
                }

                ConverterCategory& cat = g_categories[convCategory];
                int unitCount = (int)cat.units.size();

                float rowY = panelY + tabH + 30 * scale;
                Rectangle inputField = { panelX, rowY, panelW * 0.55f, 50 * scale };
                Rectangle fromUnitBtn = { panelX + panelW * 0.55f + 10 * scale, rowY, panelW * 0.45f - 10 * scale, 50 * scale };

                bool hoverInput = CheckCollisionPointRec(mousePos, inputField);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) convFieldActive = hoverInput;

                DrawRectangleRec(inputField, pal.panel);
                DrawRectangleLinesEx(inputField, 2, convFieldActive ? pal.danger : pal.borderIdle);
                std::string inputDisp = convInput.empty() ? "0" : convInput;
                DrawText(inputDisp.c_str(), (int)(inputField.x + 15 * scale), (int)(inputField.y + (inputField.height - fieldFontSize) / 2), fieldFontSize, pal.textPrimary);

                if (convFieldActive) NumericFieldInput(convInput, true, 24);

                bool hoverFromUnit = CheckCollisionPointRec(mousePos, fromUnitBtn);
                DrawRectangleRec(fromUnitBtn, hoverFromUnit ? pal.hover : pal.panel);
                std::string fromLabel = cat.units[convFromUnit].name;
                int fromW = MeasureText(fromLabel.c_str(), labelFontSize);
                DrawText(fromLabel.c_str(), (int)(fromUnitBtn.x + (fromUnitBtn.width - fromW) / 2), (int)(fromUnitBtn.y + (fromUnitBtn.height - labelFontSize) / 2), labelFontSize, pal.textPrimary);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverFromUnit) convFromUnit = (convFromUnit + 1) % unitCount;

                float arrowY = rowY + 50 * scale + 15 * scale;
                DrawText("to", (int)(panelX), (int)arrowY, labelFontSize, pal.textSecondary);

                float resultY = arrowY + labelFontSize + 10 * scale;
                Rectangle resultField = { panelX, resultY, panelW * 0.55f, 50 * scale };
                Rectangle toUnitBtn = { panelX + panelW * 0.55f + 10 * scale, resultY, panelW * 0.45f - 10 * scale, 50 * scale };

                double inputVal = 0.0;
                try { inputVal = convInput.empty() ? 0.0 : std::stod(convInput); } catch (...) { inputVal = 0.0; }
                double resultVal = ConvertValue(convCategory, convFromUnit, convToUnit, inputVal);

                DrawRectangleRec(resultField, pal.panelDark);
                std::string resultStr = FormatNumber(resultVal);
                DrawText(resultStr.c_str(), (int)(resultField.x + 15 * scale), (int)(resultField.y + (resultField.height - fieldFontSize) / 2), fieldFontSize, pal.success);

                bool hoverToUnit = CheckCollisionPointRec(mousePos, toUnitBtn);
                DrawRectangleRec(toUnitBtn, hoverToUnit ? pal.hover : pal.panel);
                std::string toLabel = cat.units[convToUnit].name;
                int toW = MeasureText(toLabel.c_str(), labelFontSize);
                DrawText(toLabel.c_str(), (int)(toUnitBtn.x + (toUnitBtn.width - toW) / 2), (int)(toUnitBtn.y + (toUnitBtn.height - labelFontSize) / 2), labelFontSize, pal.textPrimary);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverToUnit) convToUnit = (convToUnit + 1) % unitCount;

                if (convCategory == 3) {
                    DrawText("Rates are a static snapshot (no live network integration for currency).",
                        (int)panelX, (int)(resultY + 50 * scale + 15 * scale), (int)(12 * scale), pal.textSecondary);
                }
            }

            // ================================================================
            // MODE: COLOUR PICKER (EYEDROPPER + RGB SLIDERS)
            // ================================================================
            else if (mode == MODE_COLOR_PICKER) {
                int titleFontSize = (int)(30 * scale);
                int labelFontSize = (int)(16 * scale);
                int btnFontSize = (int)(17 * scale);

                DrawText("Colour Picker", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, pal.accent);

                float panelW = 420 * scale;
                if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                float panelX = contentX + (contentWidth - panelW) / 2;
                float panelY = 90 * scale;

                Rectangle pickBtn = { panelX, panelY, panelW, 50 * scale };
                bool hoverPick = CheckCollisionPointRec(mousePos, pickBtn);
                DrawRectangleRec(pickBtn, pickerActive ? pal.dangerDim : (hoverPick ? pal.hover : pal.panel));
                const char* pickLabel = pickerActive ? "Picking... click anywhere to confirm (Esc to cancel)" : "Pick Colour From Screen";
                int pickW = MeasureText(pickLabel, btnFontSize);
                if (pickW > panelW - 20 * scale) {
                    pickLabel = pickerActive ? "Picking... click to confirm" : "Pick Colour From Screen";
                    pickW = MeasureText(pickLabel, btnFontSize);
                }
                DrawText(pickLabel, (int)(pickBtn.x + (pickBtn.width - pickW) / 2), (int)(pickBtn.y + (pickBtn.height - btnFontSize) / 2), btnFontSize, pal.textPrimary);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverPick) {
                    pickerActive = !pickerActive;
                    prevGlobalMouseDown = IsLeftMouseButtonDownGlobal();
                }

                if (pickerActive) {
                    if (IsKeyPressed(KEY_ESCAPE)) {
                        pickerActive = false;
                    } else {
                        GetPixelColorAtCursor(liveR, liveG, liveB);
                        bool downNow = IsLeftMouseButtonDownGlobal();
                        if (downNow && !prevGlobalMouseDown) {
                            pickedR = liveR; pickedG = liveG; pickedB = liveB;
                            havePickedColor = true;
                            pickerActive = false;
                        }
                        prevGlobalMouseDown = downNow;
                    }
                }

                int swatchR = pickerActive ? liveR : (havePickedColor ? pickedR : 255);
                int swatchG = pickerActive ? liveG : (havePickedColor ? pickedG : 255);
                int swatchB = pickerActive ? liveB : (havePickedColor ? pickedB : 255);

                float swatchY = panelY + 50 * scale + 25 * scale;
                Rectangle swatch = { panelX, swatchY, 100 * scale, 100 * scale };
                DrawRectangleRec(swatch, Color{ (unsigned char)swatchR, (unsigned char)swatchG, (unsigned char)swatchB, 255 });
                DrawRectangleLinesEx(swatch, 2, pal.borderIdle);

                char hexBuf[16];
                snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", swatchR, swatchG, swatchB);
                char rgbBuf[32];
                snprintf(rgbBuf, sizeof(rgbBuf), "rgb(%d, %d, %d)", swatchR, swatchG, swatchB);

                float infoX = swatch.x + swatch.width + 25 * scale;
                DrawText(hexBuf, (int)infoX, (int)(swatchY), (int)(24 * scale), pal.textPrimary);
                DrawText(rgbBuf, (int)infoX, (int)(swatchY + 30 * scale), labelFontSize, pal.textSecondary);

                Rectangle copyHexBtn = { infoX, swatchY + 60 * scale, 110 * scale, 36 * scale };
                Rectangle copyRgbBtn = { infoX + 120 * scale, swatchY + 60 * scale, 110 * scale, 36 * scale };
                bool hoverCopyHex = CheckCollisionPointRec(mousePos, copyHexBtn);
                bool hoverCopyRgb = CheckCollisionPointRec(mousePos, copyRgbBtn);

                DrawRectangleRec(copyHexBtn, hoverCopyHex ? pal.hoverLight : pal.textSecondary);
                int chW = MeasureText("Copy Hex", (int)(labelFontSize * 0.9f));
                DrawText("Copy Hex", (int)(copyHexBtn.x + (copyHexBtn.width - chW) / 2), (int)(copyHexBtn.y + (copyHexBtn.height - labelFontSize * 0.9f) / 2), (int)(labelFontSize * 0.9f), pal.textOnAccent);

                DrawRectangleRec(copyRgbBtn, hoverCopyRgb ? pal.hoverLight : pal.textSecondary);
                int crW = MeasureText("Copy RGB", (int)(labelFontSize * 0.9f));
                DrawText("Copy RGB", (int)(copyRgbBtn.x + (copyRgbBtn.width - crW) / 2), (int)(copyRgbBtn.y + (copyRgbBtn.height - labelFontSize * 0.9f) / 2), (int)(labelFontSize * 0.9f), pal.textOnAccent);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverCopyHex) {
                    bool ok = CopyTextToClipboard(hexBuf);
                    copyFeedback = ok ? "Hex copied!" : "Copy failed";
                    copyFeedbackUntil = GetTime() + 1.5;
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverCopyRgb) {
                    bool ok = CopyTextToClipboard(rgbBuf);
                    copyFeedback = ok ? "RGB copied!" : "Copy failed";
                    copyFeedbackUntil = GetTime() + 1.5;
                }
                if (!copyFeedback.empty() && GetTime() < copyFeedbackUntil) {
                    DrawText(copyFeedback.c_str(), (int)infoX, (int)(copyHexBtn.y + copyHexBtn.height + 8 * scale), (int)(labelFontSize * 0.9f), pal.success);
                }

                // RGB sliders (disabled visually while actively eyedropping,
                // since the live sample is driving the colour instead).
                float slidersY = swatchY + swatch.height + 35 * scale;
                int sliderLabelSize = (int)(15 * scale);
                float sliderH = 14 * scale;
                float sliderLabelW = 20 * scale;
                float sliderValueW = 45 * scale;
                float sliderW = panelW - sliderLabelW - sliderValueW - 20 * scale;

                bool mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !pickerActive;

                const char* channelLabels[3] = { "R", "G", "B" };
                int* channelValues[3] = { &pickedR, &pickedG, &pickedB };
                Color channelColors[3] = { Color{220,60,60,255}, Color{60,190,90,255}, Color{70,120,230,255} };

                for (int c = 0; c < 3; c++) {
                    float rowY2 = slidersY + c * (sliderH + 20 * scale);
                    DrawText(channelLabels[c], (int)panelX, (int)(rowY2 - 3 * scale), sliderLabelSize, pal.textPrimary);

                    Rectangle sliderRect = { panelX + sliderLabelW, rowY2, sliderW, sliderH };
                    int valueBefore = *channelValues[c];
                    bool changed = DrawAndUpdateSlider(sliderRect, *channelValues[c], 0, 255, pal.panelDark, channelColors[c], pal.textPrimary, mousePos, mouseDown);
                    if (changed && valueBefore != *channelValues[c]) havePickedColor = true;

                    char valBuf[8];
                    snprintf(valBuf, sizeof(valBuf), "%d", *channelValues[c]);
                    DrawText(valBuf, (int)(panelX + sliderLabelW + sliderW + 10 * scale), (int)(rowY2 - 3 * scale), sliderLabelSize, pal.textPrimary);
                }

                if (pickerActive) {
                    DrawText("Sliders are disabled while picking from screen.",
                        (int)panelX, (int)(slidersY + 3 * (sliderH + 20 * scale) + 5 * scale), (int)(12 * scale), pal.textSecondary);
                }

                DrawText("Eyedropper works system-wide: move the cursor over any window while picking.",
                    (int)panelX, (int)(slidersY + 3 * (sliderH + 20 * scale) + 25 * scale), (int)(12 * scale), pal.textSecondary);
            }

            // ================================================================
            // MODE: APP LAUNCHER
            // ================================================================
            else if (mode == MODE_LAUNCHER) {
                int titleFontSize = (int)(30 * scale);
                int subTitleFontSize = (int)(14 * scale);
                int fieldFontSize = (int)(22 * scale);
                int chipFontSize = (int)(14 * scale);
                int feedbackFontSize = (int)(14 * scale);

                DrawText("App Launcher", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, pal.accent);
                DrawText("Type an app name (e.g. \"word\", \"chrome\") or a full path, then press Enter.",
                    (int)(contentX + 40 * scale), (int)(70 * scale), subTitleFontSize, pal.textSecondary);

                float fieldX = contentX + 40 * scale;
                float fieldW = contentWidth - 80 * scale;
                Rectangle searchBox = { fieldX, 105 * scale, fieldW, 55 * scale };

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    launcherFieldActive = CheckCollisionPointRec(mousePos, searchBox);
                }

                DrawRectangleRec(searchBox, pal.panel);
                DrawRectangleLinesEx(searchBox, 2, launcherFieldActive ? pal.danger : pal.borderIdle);
                std::string displayQuery = launcherQuery.empty() ? "Search or type a command..." : launcherQuery;
                Color queryColor = launcherQuery.empty() ? pal.textSecondary : pal.textPrimary;
                DrawText(displayQuery.c_str(), (int)(searchBox.x + 15 * scale), (int)(searchBox.y + (searchBox.height - fieldFontSize) / 2), fieldFontSize, queryColor);

                if (launcherFieldActive) {
                    int key = GetCharPressed();
                    while (key > 0) {
                        if (key >= 32 && key <= 125 && launcherQuery.length() < 200) launcherQuery += (char)key;
                        key = GetCharPressed();
                    }
                    if (IsKeyPressed(KEY_BACKSPACE) && !launcherQuery.empty()) launcherQuery.pop_back();
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                        if (!launcherQuery.empty()) {
                            std::string target = ResolveLaunchTarget(launcherQuery);
                            bool ok = LaunchApplication(target);
                            launcherFeedbackOk = ok;
                            launcherFeedback = ok ? ("Launched: " + target) : ("Could not launch: " + target);
                        }
                    }
                }

                float chipsY = searchBox.y + searchBox.height + 25 * scale;
                DrawText("Quick launch:", (int)fieldX, (int)chipsY, subTitleFontSize, pal.textSecondary);
                chipsY += subTitleFontSize + 12 * scale;

                float chipX = fieldX;
                float chipH = 38 * scale;
                float chipPadX = 16 * scale;
                float chipGap = 10 * scale;

                for (auto& alias : g_launchAliases) {
                    int lw = MeasureText(alias.label, chipFontSize);
                    float chipW = lw + chipPadX * 2;

                    if (chipX + chipW > fieldX + fieldW) {
                        chipX = fieldX;
                        chipsY += chipH + chipGap;
                    }

                    Rectangle chip = { chipX, chipsY, chipW, chipH };
                    bool hover = CheckCollisionPointRec(mousePos, chip);
                    DrawRectangleRec(chip, hover ? pal.hover : pal.panel);
                    DrawText(alias.label, (int)(chip.x + chipPadX), (int)(chip.y + (chip.height - chipFontSize) / 2), chipFontSize, pal.textPrimary);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover) {
                        bool ok = LaunchApplication(alias.target);
                        launcherFeedbackOk = ok;
                        launcherFeedback = ok ? (std::string("Launched: ") + alias.target) : (std::string("Could not launch: ") + alias.target);
                    }

                    chipX += chipW + chipGap;
                }

                if (!launcherFeedback.empty()) {
                    float feedbackY = chipsY + chipH + 25 * scale;
                    DrawText(launcherFeedback.c_str(), (int)fieldX, (int)feedbackY, feedbackFontSize, launcherFeedbackOk ? pal.success : pal.danger);
                }
            }

        EndDrawing();
    }

    if (g_uiFont.texture.id != GetFontDefault().texture.id) UnloadFont(g_uiFont);
    CloseWindow();
    return 0;
}