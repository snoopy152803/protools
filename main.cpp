#include "include/raylib.h"
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cctype>
#include <cstdio>
#include <algorithm>
#include <stdexcept>

#include "win32helpers.h" // OpenFileDialog/SaveFileDialog (dialogs.cpp), and the
                           // eyedropper/launcher/clipboard helpers (system_utils.cpp)

#define COLOR_REF(hex) GetColor((hex << 8) | 0xFF)

// ===========================================================================
// Custom UI font.
// raylib's built-in default font is a small (10px) bitmap font -- stretched
// up to the sizes this app uses, it looks blocky/pixelated. Loading a real
// TTF and rendering through DrawTextEx/MeasureTextEx instead fixes that.
//
// We point at Segoe UI, which ships with every Windows install, so nothing
// extra needs to be bundled. To use a different font, just change the path
// below to any .ttf on disk (e.g. one you ship alongside the .exe).
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

// Tries a list of common system-font paths and returns the first one that
// loads successfully, so the app gets a smooth TTF on Windows, macOS, and
// Linux alike -- without needing to ship a font file. Falls back to
// raylib's built-in (blocky) default font only if none of them exist.
Font LoadUIFont(int baseSize) {
    static const char* candidates[] = {
        // Windows
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        // macOS
        "/System/Library/Fonts/SFNSText.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        // Linux (common distro font packages)
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    };

    for (const char* path : candidates) {
        Font f = LoadFontEx(path, baseSize, NULL, 0);
        if (f.texture.id != 0) {
            SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
            return f;
        }
    }

    // Nothing on disk matched -- fall back to raylib's built-in font rather
    // than leaving the app without any text rendering at all.
    return GetFontDefault();
}

enum AppMode {
    MODE_WORD_COUNTER = 0,
    MODE_CALCULATOR,
    MODE_TIMER,
    MODE_CONVERTER,
    MODE_COLOR_PICKER,
    MODE_LAUNCHER,
    MODE_COUNT
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

// ===========================================================================
// Unit / currency converter data
// ===========================================================================

struct Unit { const char* name; double toBase; };

struct ConverterCategory {
    const char* name;
    std::vector<Unit> units; // ignored for Temperature, which is handled specially
};

// NOTE ON CURRENCY: these rates are a static, hand-entered snapshot (to USD)
// captured at the time this was written -- this build has no network access,
// so it cannot fetch live rates. To make this live, replace GetCurrencyRate()
// with a real HTTP call (e.g. WinHTTP on Windows) to an FX API and cache the
// result; the rest of the converter logic doesn't need to change.
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

std::string ToLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

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
    const float baseHeight = 620.0f;
    InitWindow((int)baseWidth, (int)baseHeight, "C++ Pro Toolbox");
    SetTargetFPS(60);

    // Load at a large base size (96px) so it stays sharp even when the UI's
    // biggest text (the calculator/timer displays) scales up on large
    // windows -- raylib rasterizes the glyphs once at this size and scales
    // down cleanly, but scaling up past it would look soft.
    // LoadUIFont tries several common system-font locations across Windows,
    // macOS, and Linux, and only falls back to raylib's blocky built-in
    // font if none of them are present on this machine.
    g_uiFont = LoadUIFont(96);

    AppMode mode = MODE_WORD_COUNTER;

    // --- Word counter state ---
    std::string textInput = "Type or load text here...";
    bool boxSelected = false;
    bool isAllSelected = false;

    // --- Calculator state ---
    std::string calcExpr = "";
    std::string calcDisplay = "0";
    bool calcError = false;

    // --- Timer state ---
    double timerTotalSeconds = 5 * 60.0;
    double timerRemaining = timerTotalSeconds;
    bool timerRunning = false;
    std::string timerMinutesInput = "5";
    bool timerFieldActive = false;

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

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        // -------------------------------------------------------------
        // Sidebar
        // -------------------------------------------------------------
        struct NavItem { const char* line1; const char* line2; AppMode mode; };
        static const NavItem navItems[] = {
            { "Word", "Counter", MODE_WORD_COUNTER },
            { "Calculator", nullptr, MODE_CALCULATOR },
            { "Countdown", "Timer", MODE_TIMER },
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

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int n = 0; n < navCount; n++) {
                if (CheckCollisionPointRec(mousePos, navRects[n])) mode = navItems[n].mode;
            }
        }

        float contentX = sidebarWidth;
        float contentWidth = currentWidth - sidebarWidth;

        BeginDrawing();
            ClearBackground(COLOR_REF(0x1e1e24));

            // ---------------- Sidebar ----------------
            DrawRectangleRec(sidebar, COLOR_REF(0x141419));
            DrawLine((int)sidebarWidth, 0, (int)sidebarWidth, (int)currentHeight, COLOR_REF(0x2d2d38));

            int navFontSize = (int)(14 * scale);
            for (int n = 0; n < navCount; n++) {
                bool active = (mode == navItems[n].mode);
                bool hover = CheckCollisionPointRec(mousePos, navRects[n]);
                Color col = active ? MAROON : (hover ? COLOR_REF(0x3a3a46) : COLOR_REF(0x2d2d38));
                DrawRectangleRec(navRects[n], col);

                if (navItems[n].line2) {
                    int lineH = navFontSize + 2;
                    int w1 = MeasureText(navItems[n].line1, navFontSize);
                    int w2 = MeasureText(navItems[n].line2, navFontSize);
                    float blockH = (float)(lineH * 2);
                    float startY = navRects[n].y + (navRects[n].height - blockH) / 2;
                    DrawText(navItems[n].line1, (int)(navRects[n].x + (navRects[n].width - w1) / 2), (int)startY, navFontSize, RAYWHITE);
                    DrawText(navItems[n].line2, (int)(navRects[n].x + (navRects[n].width - w2) / 2), (int)(startY + lineH), navFontSize, RAYWHITE);
                } else {
                    int w = MeasureText(navItems[n].line1, navFontSize);
                    DrawText(navItems[n].line1, (int)(navRects[n].x + (navRects[n].width - w) / 2), (int)(navRects[n].y + (navRects[n].height - navFontSize) / 2), navFontSize, RAYWHITE);
                }
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

                DrawText("Word Counter", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, MAROON);
                DrawText("Press F11 to Toggle Fullscreen | Ctrl+A: Select All", (int)(contentX + 40 * scale), (int)(70 * scale), subTitleFontSize, GRAY);

                DrawRectangleRec(inputBox, COLOR_REF(0x2d2d38));
                DrawRectangleLinesEx(inputBox, 2, boxSelected ? RED : DARKGRAY);

                float padX = 15 * scale;
                float padY = 15 * scale;

                if (textInput.empty() && !boxSelected) {
                    DrawText("Type or load text here...", (int)(inputBox.x + padX), (int)(inputBox.y + padY), inputFontSize, DARKGRAY);
                } else if (isAllSelected && !textInput.empty()) {
                    int lineY = (int)(inputBox.y + padY);
                    for (const auto& line : wrappedLines) {
                        if (lineY + inputFontSize < inputBox.y + inputBox.height) {
                            int textW = MeasureText(line.c_str(), inputFontSize);
                            DrawRectangle((int)(inputBox.x + padX), lineY, textW, inputFontSize + 4, GetColor(0x3a7bd588));
                            DrawText(line.c_str(), (int)(inputBox.x + padX), lineY, inputFontSize, RAYWHITE);
                        }
                        lineY += lineSpacing;
                    }
                } else {
                    int lineY = (int)(inputBox.y + padY);
                    for (const auto& line : wrappedLines) {
                        if (lineY + inputFontSize < inputBox.y + inputBox.height) {
                            DrawText(line.c_str(), (int)(inputBox.x + padX), lineY, inputFontSize, RAYWHITE);
                        }
                        lineY += lineSpacing;
                    }
                    if (boxSelected && ((GetTime() * 2) - (int)(GetTime() * 2) > 0.5)) {
                        int lastLineW = wrappedLines.empty() ? 0 : MeasureText(wrappedLines.back().c_str(), inputFontSize);
                        int cursorY = wrappedLines.empty() ? (int)(inputBox.y + padY) : (int)(inputBox.y + padY + (wrappedLines.size() - 1) * lineSpacing);
                        if (cursorY < inputBox.y + inputBox.height - lineSpacing) {
                            DrawRectangle((int)(inputBox.x + padX + lastLineW), cursorY, 2, inputFontSize, RED);
                        }
                    }
                }

                bool hoverSave = CheckCollisionPointRec(mousePos, saveBtn);
                bool hoverLoad = CheckCollisionPointRec(mousePos, loadBtn);
                DrawRectangleRec(saveBtn, hoverSave ? LIGHTGRAY : GRAY);
                int saveTxtW = MeasureText("Save As...", btnFontSize);
                DrawText("Save As...", (int)(saveBtn.x + (saveBtn.width - saveTxtW) / 2), (int)(saveBtn.y + (saveBtn.height - btnFontSize) / 2), btnFontSize, WHITE);
                DrawRectangleRec(loadBtn, hoverLoad ? LIGHTGRAY : GRAY);
                int loadTxtW = MeasureText("Open File", btnFontSize);
                DrawText("Open File", (int)(loadBtn.x + (loadBtn.width - loadTxtW) / 2), (int)(loadBtn.y + (loadBtn.height - btnFontSize) / 2), btnFontSize, WHITE);

                DrawRectangleRec(dashboard, COLOR_REF(0x141419));
                std::string wordsStr = "Words: " + std::to_string(wordCount);
                std::string charsStr = "Characters: " + std::to_string(charCount);
                DrawText(wordsStr.c_str(), (int)(dashboard.x + 40 * scale), (int)(dashboard.y + (dashboard.height - statsFontSize) / 2), statsFontSize, GREEN);
                DrawText(charsStr.c_str(), (int)(dashboard.x + dashboard.width / 2), (int)(dashboard.y + (dashboard.height - statsFontSize) / 2), statsFontSize, ORANGE);
            }

            // ================================================================
            // MODE: CALCULATOR
            // ================================================================
            else if (mode == MODE_CALCULATOR) {
                int titleFontSize = (int)(30 * scale);
                int displayFontSize = (int)(34 * scale);
                int exprFontSize = (int)(16 * scale);
                int keyFontSize = (int)(22 * scale);

                DrawText("Calculator", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, MAROON);

                float panelW = 360 * scale;
                if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                float panelX = contentX + (contentWidth - panelW) / 2;
                float panelY = 90 * scale;

                Rectangle displayBox = { panelX, panelY, panelW, 90 * scale };
                DrawRectangleRec(displayBox, COLOR_REF(0x2d2d38));
                DrawRectangleLinesEx(displayBox, 2, DARKGRAY);

                int exprW = MeasureText(calcExpr.c_str(), exprFontSize);
                DrawText(calcExpr.c_str(), (int)(displayBox.x + displayBox.width - 15 * scale - exprW), (int)(displayBox.y + 10 * scale), exprFontSize, GRAY);

                Color displayColor = calcError ? RED : RAYWHITE;
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
                        Color base = isEquals ? MAROON : (isClear ? COLOR_REF(0x8a3030) : (isOp ? COLOR_REF(0x3a3a46) : COLOR_REF(0x2d2d38)));
                        Color col = hover ? ColorBrightness(base, 0.25f) : base;

                        DrawRectangleRec(btn, col);
                        int lw = MeasureText(label, keyFontSize);
                        DrawText(label, (int)(btn.x + (btn.width - lw) / 2), (int)(btn.y + (btn.height - keyFontSize) / 2), keyFontSize, RAYWHITE);

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
            // MODE: COUNTDOWN TIMER
            // ================================================================
            else if (mode == MODE_TIMER) {
                int titleFontSize = (int)(30 * scale);
                int bigFontSize = (int)(64 * scale);
                int labelFontSize = (int)(16 * scale);
                int btnFontSize = (int)(17 * scale);

                DrawText("Countdown Timer", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, MAROON);

                if (timerRunning) {
                    timerRemaining -= GetFrameTime();
                    if (timerRemaining <= 0.0) { timerRemaining = 0.0; timerRunning = false; }
                }

                float panelW = 420 * scale;
                if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                float panelX = contentX + (contentWidth - panelW) / 2;

                // Big MM:SS display
                int totalSecs = (int)(timerRemaining + 0.5);
                int mm = totalSecs / 60;
                int ss = totalSecs % 60;
                char timeBuf[16];
                snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mm, ss);

                bool finished = (timerRemaining <= 0.0) && !timerRunning && timerTotalSeconds > 0.0;
                bool blink = finished && (((GetTime() * 2) - (int)(GetTime() * 2)) > 0.5);
                Color timeColor = finished ? (blink ? RED : COLOR_REF(0x8a3030)) : RAYWHITE;

                int timeW = MeasureText(timeBuf, bigFontSize);
                DrawText(timeBuf, (int)(panelX + (panelW - timeW) / 2), (int)(100 * scale), bigFontSize, timeColor);

                if (finished) {
                    const char* msg = "Time's up!";
                    int msgW = MeasureText(msg, labelFontSize);
                    DrawText(msg, (int)(panelX + (panelW - msgW) / 2), (int)(100 * scale + bigFontSize + 10 * scale), labelFontSize, RED);
                }

                // Preset buttons
                const int presets[] = { 1, 5, 10, 15, 30 };
                int presetCount = 5;
                float presetY = 100 * scale + bigFontSize + 45 * scale;
                float presetGap = 10 * scale;
                float presetW = (panelW - presetGap * (presetCount - 1)) / presetCount;
                float presetH = 40 * scale;

                for (int p = 0; p < presetCount; p++) {
                    Rectangle btn = { panelX + p * (presetW + presetGap), presetY, presetW, presetH };
                    bool hover = CheckCollisionPointRec(mousePos, btn);
                    DrawRectangleRec(btn, hover ? COLOR_REF(0x3a3a46) : COLOR_REF(0x2d2d38));
                    std::string label = std::to_string(presets[p]) + "m";
                    int lw = MeasureText(label.c_str(), btnFontSize);
                    DrawText(label.c_str(), (int)(btn.x + (btn.width - lw) / 2), (int)(btn.y + (btn.height - btnFontSize) / 2), btnFontSize, RAYWHITE);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover) {
                        timerTotalSeconds = presets[p] * 60.0;
                        timerRemaining = timerTotalSeconds;
                        timerRunning = false;
                        timerMinutesInput = std::to_string(presets[p]);
                    }
                }

                // Custom minutes field + Set button
                float fieldY = presetY + presetH + 25 * scale;
                Rectangle fieldBox = { panelX, fieldY, 120 * scale, 45 * scale };
                Rectangle setBtn = { panelX + 130 * scale, fieldY, 100 * scale, 45 * scale };

                bool hoverField = CheckCollisionPointRec(mousePos, fieldBox);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) timerFieldActive = hoverField;

                DrawRectangleRec(fieldBox, COLOR_REF(0x2d2d38));
                DrawRectangleLinesEx(fieldBox, 2, timerFieldActive ? RED : DARKGRAY);
                std::string fieldDisp = timerMinutesInput.empty() ? "0" : timerMinutesInput;
                DrawText(fieldDisp.c_str(), (int)(fieldBox.x + 12 * scale), (int)(fieldBox.y + (fieldBox.height - btnFontSize) / 2), btnFontSize, RAYWHITE);
                DrawText("min", (int)(fieldBox.x + fieldBox.width + 8 * scale), (int)(fieldBox.y + (fieldBox.height - labelFontSize) / 2), labelFontSize, GRAY);

                if (timerFieldActive) {
                    int key = GetCharPressed();
                    while (key > 0) {
                        if (key >= '0' && key <= '9' && timerMinutesInput.length() < 4) timerMinutesInput += (char)key;
                        key = GetCharPressed();
                    }
                    if (IsKeyPressed(KEY_BACKSPACE) && !timerMinutesInput.empty()) timerMinutesInput.pop_back();
                }

                bool hoverSet = CheckCollisionPointRec(mousePos, setBtn);
                DrawRectangleRec(setBtn, hoverSet ? LIGHTGRAY : GRAY);
                int setW = MeasureText("Set", btnFontSize);
                DrawText("Set", (int)(setBtn.x + (setBtn.width - setW) / 2), (int)(setBtn.y + (setBtn.height - btnFontSize) / 2), btnFontSize, WHITE);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverSet && !timerMinutesInput.empty()) {
                    int mins = atoi(timerMinutesInput.c_str());
                    timerTotalSeconds = mins * 60.0;
                    timerRemaining = timerTotalSeconds;
                    timerRunning = false;
                }

                // Start/Pause and Reset
                float ctrlY = fieldY + 45 * scale + 25 * scale;
                Rectangle startBtn = { panelX, ctrlY, panelW / 2 - 10 * scale, 50 * scale };
                Rectangle resetBtn = { panelX + panelW / 2 + 10 * scale, ctrlY, panelW / 2 - 10 * scale, 50 * scale };

                bool hoverStart = CheckCollisionPointRec(mousePos, startBtn);
                bool hoverReset = CheckCollisionPointRec(mousePos, resetBtn);

                DrawRectangleRec(startBtn, timerRunning ? COLOR_REF(0x8a3030) : (hoverStart ? ColorBrightness(GREEN, -0.3f) : COLOR_REF(0x2d5a2d)));
                const char* startLabel = timerRunning ? "Pause" : "Start";
                int startW = MeasureText(startLabel, btnFontSize);
                DrawText(startLabel, (int)(startBtn.x + (startBtn.width - startW) / 2), (int)(startBtn.y + (startBtn.height - btnFontSize) / 2), btnFontSize, WHITE);

                DrawRectangleRec(resetBtn, hoverReset ? LIGHTGRAY : GRAY);
                int resetW = MeasureText("Reset", btnFontSize);
                DrawText("Reset", (int)(resetBtn.x + (resetBtn.width - resetW) / 2), (int)(resetBtn.y + (resetBtn.height - btnFontSize) / 2), btnFontSize, WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (hoverStart && timerRemaining > 0.0) timerRunning = !timerRunning;
                    if (hoverReset) { timerRemaining = timerTotalSeconds; timerRunning = false; }
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

                DrawText("Unit & Currency Converter", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, MAROON);

                float panelW = 460 * scale;
                if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                float panelX = contentX + (contentWidth - panelW) / 2;
                float panelY = 90 * scale;

                // Category tabs
                int catCount = (int)g_categories.size();
                float tabGap = 8 * scale;
                float tabW = (panelW - tabGap * (catCount - 1)) / catCount;
                float tabH = 38 * scale;

                for (int i = 0; i < catCount; i++) {
                    Rectangle tabBtn = { panelX + i * (tabW + tabGap), panelY, tabW, tabH };
                    bool active = (i == convCategory);
                    bool hover = CheckCollisionPointRec(mousePos, tabBtn);
                    DrawRectangleRec(tabBtn, active ? MAROON : (hover ? COLOR_REF(0x3a3a46) : COLOR_REF(0x2d2d38)));

                    // Wrap long category names onto the tab reasonably: just shrink to fit.
                    std::string label = g_categories[i].name;
                    int lw = MeasureText(label.c_str(), tabFontSize);
                    DrawText(label.c_str(), (int)(tabBtn.x + (tabBtn.width - lw) / 2), (int)(tabBtn.y + (tabBtn.height - tabFontSize) / 2), tabFontSize, RAYWHITE);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover && !active) {
                        convCategory = i;
                        convFromUnit = 0;
                        convToUnit = (int)g_categories[i].units.size() > 1 ? 1 : 0;
                    }
                }

                ConverterCategory& cat = g_categories[convCategory];
                int unitCount = (int)cat.units.size();

                // Input value field
                float rowY = panelY + tabH + 30 * scale;
                Rectangle inputField = { panelX, rowY, panelW * 0.55f, 50 * scale };
                Rectangle fromUnitBtn = { panelX + panelW * 0.55f + 10 * scale, rowY, panelW * 0.45f - 10 * scale, 50 * scale };

                bool hoverInput = CheckCollisionPointRec(mousePos, inputField);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) convFieldActive = hoverInput;

                DrawRectangleRec(inputField, COLOR_REF(0x2d2d38));
                DrawRectangleLinesEx(inputField, 2, convFieldActive ? RED : DARKGRAY);
                std::string inputDisp = convInput.empty() ? "0" : convInput;
                DrawText(inputDisp.c_str(), (int)(inputField.x + 15 * scale), (int)(inputField.y + (inputField.height - fieldFontSize) / 2), fieldFontSize, RAYWHITE);

                if (convFieldActive) NumericFieldInput(convInput, true, 24);

                bool hoverFromUnit = CheckCollisionPointRec(mousePos, fromUnitBtn);
                DrawRectangleRec(fromUnitBtn, hoverFromUnit ? COLOR_REF(0x3a3a46) : COLOR_REF(0x2d2d38));
                std::string fromLabel = cat.units[convFromUnit].name;
                int fromW = MeasureText(fromLabel.c_str(), labelFontSize);
                DrawText(fromLabel.c_str(), (int)(fromUnitBtn.x + (fromUnitBtn.width - fromW) / 2), (int)(fromUnitBtn.y + (fromUnitBtn.height - labelFontSize) / 2), labelFontSize, RAYWHITE);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverFromUnit) convFromUnit = (convFromUnit + 1) % unitCount;

                // Arrow
                float arrowY = rowY + 50 * scale + 15 * scale;
                DrawText("to", (int)(panelX), (int)arrowY, labelFontSize, GRAY);

                // Result row
                float resultY = arrowY + labelFontSize + 10 * scale;
                Rectangle resultField = { panelX, resultY, panelW * 0.55f, 50 * scale };
                Rectangle toUnitBtn = { panelX + panelW * 0.55f + 10 * scale, resultY, panelW * 0.45f - 10 * scale, 50 * scale };

                double inputVal = 0.0;
                try { inputVal = convInput.empty() ? 0.0 : std::stod(convInput); } catch (...) { inputVal = 0.0; }
                double resultVal = ConvertValue(convCategory, convFromUnit, convToUnit, inputVal);

                DrawRectangleRec(resultField, COLOR_REF(0x141419));
                std::string resultStr = FormatNumber(resultVal);
                DrawText(resultStr.c_str(), (int)(resultField.x + 15 * scale), (int)(resultField.y + (resultField.height - fieldFontSize) / 2), fieldFontSize, GREEN);

                bool hoverToUnit = CheckCollisionPointRec(mousePos, toUnitBtn);
                DrawRectangleRec(toUnitBtn, hoverToUnit ? COLOR_REF(0x3a3a46) : COLOR_REF(0x2d2d38));
                std::string toLabel = cat.units[convToUnit].name;
                int toW = MeasureText(toLabel.c_str(), labelFontSize);
                DrawText(toLabel.c_str(), (int)(toUnitBtn.x + (toUnitBtn.width - toW) / 2), (int)(toUnitBtn.y + (toUnitBtn.height - labelFontSize) / 2), labelFontSize, RAYWHITE);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverToUnit) convToUnit = (convToUnit + 1) % unitCount;

                if (convCategory == 3) { // Currency
                    DrawText("Rates are a static snapshot (no live network access in this build).",
                        (int)panelX, (int)(resultY + 50 * scale + 15 * scale), (int)(12 * scale), GRAY);
                }
            }

            // ================================================================
            // MODE: COLOUR PICKER (EYEDROPPER)
            // ================================================================
            else if (mode == MODE_COLOR_PICKER) {
                int titleFontSize = (int)(30 * scale);
                int labelFontSize = (int)(16 * scale);
                int btnFontSize = (int)(17 * scale);

                DrawText("Colour Picker", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, MAROON);

                float panelW = 380 * scale;
                if (panelW > contentWidth - 60 * scale) panelW = contentWidth - 60 * scale;
                float panelX = contentX + (contentWidth - panelW) / 2;
                float panelY = 100 * scale;

                // Eyedropper toggle button
                Rectangle pickBtn = { panelX, panelY, panelW, 50 * scale };
                bool hoverPick = CheckCollisionPointRec(mousePos, pickBtn);
                DrawRectangleRec(pickBtn, pickerActive ? COLOR_REF(0x8a3030) : (hoverPick ? COLOR_REF(0x3a3a46) : COLOR_REF(0x2d2d38)));
                const char* pickLabel = pickerActive ? "Picking... click anywhere to confirm (Esc to cancel)" : "Pick Colour From Screen";
                int pickW = MeasureText(pickLabel, btnFontSize);
                if (pickW > panelW - 20 * scale) {
                    // Fallback shorter label if it doesn't fit at this scale.
                    pickLabel = pickerActive ? "Picking... click to confirm" : "Pick Colour From Screen";
                    pickW = MeasureText(pickLabel, btnFontSize);
                }
                DrawText(pickLabel, (int)(pickBtn.x + (pickBtn.width - pickW) / 2), (int)(pickBtn.y + (pickBtn.height - btnFontSize) / 2), btnFontSize, RAYWHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverPick) {
                    pickerActive = !pickerActive;
                    prevGlobalMouseDown = IsLeftMouseButtonDownGlobal(); // avoid instantly re-triggering on the same click
                }

                if (pickerActive) {
                    if (IsKeyPressed(KEY_ESCAPE)) {
                        pickerActive = false;
                    } else {
                        if (GetPixelColorAtCursor(liveR, liveG, liveB)) {
                            // live preview updates continuously, even while hovering other windows
                        }
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
                Rectangle swatch = { panelX, swatchY, 110 * scale, 110 * scale };
                DrawRectangleRec(swatch, Color{ (unsigned char)swatchR, (unsigned char)swatchG, (unsigned char)swatchB, 255 });
                DrawRectangleLinesEx(swatch, 2, DARKGRAY);

                char hexBuf[16];
                snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", swatchR, swatchG, swatchB);
                std::string rgbStr = "RGB(" + std::to_string(swatchR) + ", " + std::to_string(swatchG) + ", " + std::to_string(swatchB) + ")";

                float infoX = swatch.x + swatch.width + 25 * scale;
                DrawText(hexBuf, (int)infoX, (int)(swatchY + 10 * scale), (int)(24 * scale), RAYWHITE);
                DrawText(rgbStr.c_str(), (int)infoX, (int)(swatchY + 10 * scale + 30 * scale), labelFontSize, GRAY);

                Rectangle copyBtn = { infoX, swatchY + 70 * scale, 150 * scale, 40 * scale };
                bool hoverCopy = CheckCollisionPointRec(mousePos, copyBtn);
                DrawRectangleRec(copyBtn, hoverCopy ? LIGHTGRAY : GRAY);
                int copyW = MeasureText("Copy Hex", (int)(labelFontSize));
                DrawText("Copy Hex", (int)(copyBtn.x + (copyBtn.width - copyW) / 2), (int)(copyBtn.y + (copyBtn.height - labelFontSize) / 2), labelFontSize, WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverCopy) {
                    bool ok = CopyTextToClipboard(hexBuf);
                    copyFeedback = ok ? "Copied!" : "Copy failed";
                    copyFeedbackUntil = GetTime() + 1.5;
                }
                if (!copyFeedback.empty() && GetTime() < copyFeedbackUntil) {
                    DrawText(copyFeedback.c_str(), (int)copyBtn.x, (int)(copyBtn.y + copyBtn.height + 8 * scale), (int)(labelFontSize), GREEN);
                }

                DrawText("Works system-wide: move the cursor over any window while picking.",
                    (int)panelX, (int)(swatchY + swatch.height + 20 * scale), (int)(12 * scale), GRAY);
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

                DrawText("App Launcher", (int)(contentX + 40 * scale), (int)(30 * scale), titleFontSize, MAROON);
                DrawText("Type an app name (e.g. \"word\", \"chrome\") or a full path, then press Enter.",
                    (int)(contentX + 40 * scale), (int)(70 * scale), subTitleFontSize, GRAY);

                float fieldX = contentX + 40 * scale;
                float fieldW = contentWidth - 80 * scale;
                Rectangle searchBox = { fieldX, 105 * scale, fieldW, 55 * scale };

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    launcherFieldActive = CheckCollisionPointRec(mousePos, searchBox);
                }

                DrawRectangleRec(searchBox, COLOR_REF(0x2d2d38));
                DrawRectangleLinesEx(searchBox, 2, launcherFieldActive ? RED : DARKGRAY);
                std::string displayQuery = launcherQuery.empty() ? "Search or type a command..." : launcherQuery;
                Color queryColor = launcherQuery.empty() ? DARKGRAY : RAYWHITE;
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

                // Quick-launch chips
                float chipsY = searchBox.y + searchBox.height + 25 * scale;
                DrawText("Quick launch:", (int)fieldX, (int)chipsY, subTitleFontSize, GRAY);
                chipsY += subTitleFontSize + 12 * scale;

                float chipX = fieldX;
                float chipH = 38 * scale;
                float chipPadX = 16 * scale;
                float chipGap = 10 * scale;

                for (auto& alias : g_launchAliases) {
                    int lw = MeasureText(alias.label, chipFontSize);
                    float chipW = lw + chipPadX * 2;

                    if (chipX + chipW > fieldX + fieldW) { // wrap to next row
                        chipX = fieldX;
                        chipsY += chipH + chipGap;
                    }

                    Rectangle chip = { chipX, chipsY, chipW, chipH };
                    bool hover = CheckCollisionPointRec(mousePos, chip);
                    DrawRectangleRec(chip, hover ? COLOR_REF(0x3a3a46) : COLOR_REF(0x2d2d38));
                    DrawText(alias.label, (int)(chip.x + chipPadX), (int)(chip.y + (chip.height - chipFontSize) / 2), chipFontSize, RAYWHITE);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover) {
                        bool ok = LaunchApplication(alias.target);
                        launcherFeedbackOk = ok;
                        launcherFeedback = ok ? (std::string("Launched: ") + alias.target) : (std::string("Could not launch: ") + alias.target);
                    }

                    chipX += chipW + chipGap;
                }

                if (!launcherFeedback.empty()) {
                    float feedbackY = chipsY + chipH + 25 * scale;
                    DrawText(launcherFeedback.c_str(), (int)fieldX, (int)feedbackY, feedbackFontSize, launcherFeedbackOk ? GREEN : RED);
                }
            }

        EndDrawing();
    }

    if (g_uiFont.texture.id != GetFontDefault().texture.id) UnloadFont(g_uiFont);
    CloseWindow();
    return 0;
}
