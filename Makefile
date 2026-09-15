# Makefile for C++ Pro Toolbox (+ launcher.exe / unavailable.exe)
#
# Works with any g++/clang++-compatible compiler on Windows (MinGW-w64, or
# clang targeting MinGW). Override CXX on the command line to switch
# compilers without editing this file, e.g.:
#
#   make                 # builds all three exes with g++ (the default)
#   make CXX=clang++     # build with clang instead
#   make CXX=g++-13      # build with a specific g++ version
#
# Three separate executables come out of this:
#   launcher.exe    -- what you actually run/pin. Looks for toolbox.exe next
#                       to itself; if missing, runs unavailable.exe instead.
#   toolbox.exe     -- the real app (main.cpp + dialogs.cpp + system_utils.cpp)
#   unavailable.exe -- the "sorry, not available" fallback dialog
#
# MSVC (cl.exe) is not driven by this Makefile -- its command-line syntax
# is incompatible with GNU Make's implicit rules. See the "Visual Studio /
# MSVC" section in README.md for building with MSVC instead.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Iinclude
DEPFLAGS  = -MMD -MP
# -mwindows builds a GUI-subsystem executable instead of a console one, so no
# black console window pops up behind any of these when launched. This is
# also what makes linking launcher.exe/unavailable.exe work at all -- they
# only define WinMain(), not main(), and -mwindows is what tells the linker
# to look for a WinMain entry point instead of a console main(). Override
# with LDFLAGS= (empty) if you want the console back for debugging toolbox.exe.
LDFLAGS  ?= -mwindows

WINDRES  ?= windres

# ---------------------------------------------------------------------------
# toolbox.exe -- the real app
# ---------------------------------------------------------------------------
TOOLBOX_TARGET  := toolbox.exe
TOOLBOX_SOURCES := main.cpp dialogs.cpp system_utils.cpp
TOOLBOX_OBJECTS := $(TOOLBOX_SOURCES:.cpp=.o)
TOOLBOX_LIBS    := -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lcomdlg32 -lwinhttp
RESOURCE        := icon.res

# ---------------------------------------------------------------------------
# launcher.exe -- checks for toolbox.exe next to itself, falls back to
# unavailable.exe. Needs Shell32 (ShellExecuteW) and std::filesystem.
# ---------------------------------------------------------------------------
LAUNCHER_TARGET  := launcher.exe
LAUNCHER_SOURCES := launcher.cpp
LAUNCHER_OBJECTS := $(LAUNCHER_SOURCES:.cpp=.o)
LAUNCHER_LIBS    := -lshell32
# Older MinGW-w64 (GCC < 9) shipped std::filesystem as a separate library.
# If you get "undefined reference to std::filesystem::..." at link time,
# uncomment the next line:
# LAUNCHER_LIBS   += -lstdc++fs

# ---------------------------------------------------------------------------
# unavailable.exe -- the fallback dialog. Needs User32 (MessageBoxW); usually
# already pulled in transitively, listed explicitly to be safe.
# ---------------------------------------------------------------------------
UNAVAILABLE_TARGET  := unavailable.exe
UNAVAILABLE_SOURCES := unavailable.cpp
UNAVAILABLE_OBJECTS := $(UNAVAILABLE_SOURCES:.cpp=.o)
UNAVAILABLE_LIBS    := -luser32

ALL_SOURCES := $(TOOLBOX_SOURCES) $(LAUNCHER_SOURCES) $(UNAVAILABLE_SOURCES)
ALL_DEPS    := $(ALL_SOURCES:.cpp=.d)

.PHONY: all clean toolbox launcher unavailable

all: $(TOOLBOX_TARGET) $(LAUNCHER_TARGET) $(UNAVAILABLE_TARGET)

toolbox: $(TOOLBOX_TARGET)
launcher: $(LAUNCHER_TARGET)
unavailable: $(UNAVAILABLE_TARGET)

$(TOOLBOX_TARGET): $(TOOLBOX_OBJECTS) $(RESOURCE)
	$(CXX) $(LDFLAGS) $(TOOLBOX_OBJECTS) $(RESOURCE) -o $@ $(TOOLBOX_LIBS)

$(LAUNCHER_TARGET): $(LAUNCHER_OBJECTS) $(RESOURCE)
	$(CXX) $(LDFLAGS) $(LAUNCHER_OBJECTS) $(RESOURCE) -o $@ $(LAUNCHER_LIBS)

$(UNAVAILABLE_TARGET): $(UNAVAILABLE_OBJECTS) $(RESOURCE)
	$(CXX) $(LDFLAGS) $(UNAVAILABLE_OBJECTS) $(RESOURCE) -o $@ $(UNAVAILABLE_LIBS)

# Auto-generated .d files (via -MMD -MP) track each .cpp's actual #include
# graph, so editing win32helpers.h (or raylib.h) correctly triggers a
# rebuild of whichever .o files include it -- without hardcoding that
# dependency here per-compiler. One shared pattern rule covers all three
# targets' .cpp files.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(RESOURCE): icon.rc appicon.ico
	$(WINDRES) icon.rc -O coff -o $@

-include $(ALL_DEPS)

clean:
	rm -f $(TOOLBOX_OBJECTS) $(LAUNCHER_OBJECTS) $(UNAVAILABLE_OBJECTS) $(ALL_DEPS) $(RESOURCE) \
	      $(TOOLBOX_TARGET) $(LAUNCHER_TARGET) $(UNAVAILABLE_TARGET)