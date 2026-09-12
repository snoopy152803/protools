# Compilation commands
**Make sure you cd to the right folder first. For me, cd command is below.**
```sh
cd "C:\Users\rsong\OneDrive\Documents\Websites\DesktopApps
```
## Create icon.o
```sh
windres icon.rc -o icon.o   # to create icon.o for taskbar icon, optional
```
## Create executable
```sh
g++ main.cpp dialogs.cpp systemutils.cpp icon.o -o app.exe -Iinclude -Llib -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -mwindows # main compilation command
```
*NOTE: The first command is optional, while the second command is mandatory when you edit the file.*