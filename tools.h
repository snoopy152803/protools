#ifndef TOOLS_H
#define TOOLS_H

#include <string>

// Blueprints from dialogs.cpp
std::string OpenFileDialog();
std::string SaveFileDialog();

// Blueprints from systemutils.cpp
bool GetPixelColorAtCursor(int& outR, int& outG, int& outB);
bool IsLeftMouseButtonDownGlobal();
bool LaunchApplication(const std::string& target);
bool CopyTextToClipboard(const std::string& text);

#endif // TOOLS_H
