# Makefile for C++ Pro Toolbox
#
# Works with any g++/clang++-compatible compiler on Windows (MinGW-w64, or
# clang targeting MinGW). Override CXX on the command line to switch
# compilers without editing this file, e.g.:
#
#   make                 # uses g++ (the default below)
#   make CXX=clang++     # build with clang instead
#   make CXX=g++-13      # build with a specific g++ version
#
# MSVC (cl.exe) is not driven by this Makefile -- its command-line syntax
# is incompatible with GNU Make's implicit rules. See the "Visual Studio /
# MSVC" section in README.md for building with MSVC instead.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Iinclude
DEPFLAGS  = -MMD -MP
LDLIBS   ?= -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lcomdlg32

WINDRES  ?= windres

TARGET   := toolbox.exe
SOURCES  := main.cpp dialogs.cpp system_utils.cpp
OBJECTS  := $(SOURCES:.cpp=.o)
DEPS     := $(SOURCES:.cpp=.d)
RESOURCE := icon.res

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) $(RESOURCE)
	$(CXX) $(OBJECTS) $(RESOURCE) -o $@ $(LDLIBS)

# Auto-generated .d files (via -MMD -MP) track each .cpp's actual #include
# graph, so editing win32helpers.h (or raylib.h) correctly triggers a
# rebuild of whichever .o files include it -- without hardcoding that
# dependency here per-compiler.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(RESOURCE): icon.rc appicon.ico
	$(WINDRES) icon.rc -O coff -o $@

-include $(DEPS)

clean:
	rm -f $(OBJECTS) $(DEPS) $(RESOURCE) $(TARGET)
