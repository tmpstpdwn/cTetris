VERSION := 1.0.0
CC = gcc
SRC := $(wildcard src/*.c)
COMMON_CFLAGS := -Iinclude -Wall -Wextra -pedantic -std=c99

ifdef DEBUG
    BUILD_CFLAGS := -g -O0 -DDEBUG
    BUILD_LDFLAGS :=
else
    BUILD_CFLAGS := -O3 -flto
    BUILD_LDFLAGS := -flto -s
endif

.DEFAULT_GOAL := help
help:
	@echo "Make what???"
	@echo ""
	@echo "make linux                   - Linux release build"
	@echo "make windows                 - Windows release build"
	@echo "make linux DEBUG=1           - Linux debug build"
	@echo "make windows DEBUG=1         - Windows debug build"
	@echo "make package PACKAGE=linux   - Package Linux release build (.tar.gz)"
	@echo "make package PACKAGE=windows - Package Windows release build (.zip)"
	@echo "make clean                   - Remove build artifacts"

# Linux

LINUX_BUILD_DIR := build/linux
LINUX_OBJ := $(patsubst src/%.c,$(LINUX_BUILD_DIR)/%.o,$(SRC))
LINUX_OUT := cTetris$(if $(DEBUG),_debug)
LINUX_CFLAGS := $(BUILD_CFLAGS)
LINUX_LDFLAGS := $(BUILD_LDFLAGS)

ifdef DEBUG
    LINUX_CFLAGS += -fno-omit-frame-pointer -fsanitize=address
    LINUX_LDFLAGS += -fsanitize=address
endif

$(LINUX_BUILD_DIR):
	mkdir -p $@

$(LINUX_BUILD_DIR)/%.o: src/%.c | $(LINUX_BUILD_DIR)
	$(CC) -c $< -o $@ $(COMMON_CFLAGS) $(LINUX_CFLAGS)

linux: $(LINUX_OBJ)
	$(CC) $(LINUX_OBJ) -o $(LINUX_OUT) $(LINUX_LDFLAGS) -Llib -lX11 -lm -lraylib

# Windows

WINDOWS_CC := x86_64-w64-mingw32-gcc
WINDOWS_RC := x86_64-w64-mingw32-windres
WINDOWS_BUILD_DIR := build/windows
WINDOWS_OBJ := $(patsubst src/%.c,$(WINDOWS_BUILD_DIR)/%.o,$(SRC))
WINDOWS_RES := $(WINDOWS_BUILD_DIR)/resource.o
WINDOWS_OUT := cTetris$(if $(DEBUG),_debug).exe
WINDOWS_CFLAGS := $(BUILD_CFLAGS)
WINDOWS_LDFLAGS := $(BUILD_LDFLAGS)

$(WINDOWS_BUILD_DIR):
	mkdir -p $@

$(WINDOWS_BUILD_DIR)/%.o: src/%.c | $(WINDOWS_BUILD_DIR)
	$(WINDOWS_CC) -c $< -o $@ $(COMMON_CFLAGS) $(WINDOWS_CFLAGS)

$(WINDOWS_RES): resource.rc | $(WINDOWS_BUILD_DIR)
	$(WINDOWS_RC) $< -O coff -o $@

windows: $(WINDOWS_OBJ) $(WINDOWS_RES)
	$(WINDOWS_CC) $(WINDOWS_OBJ) $(WINDOWS_RES) -o $(WINDOWS_OUT) \
	$(WINDOWS_LDFLAGS) -Llib_win -lraylib -lwinmm \
	-Wl,--defsym=stat64i32=_stat64i32 -lgdi32 -lopengl32 \
	$(if $(DEBUG),,-mwindows)

# Package

package:
ifndef PACKAGE
	@echo "Error: PACKAGE not specified"
	@echo "Usage: make package PACKAGE=linux   (for .tar.gz)"
	@echo "   or: make package PACKAGE=windows (for .zip)"
	@exit 1
endif
ifeq ($(PACKAGE),linux)
	$(MAKE) clean
	$(MAKE) linux DEBUG=
	rm -rf cTetris-$(VERSION)-linux-x86_64
	mkdir -p cTetris-$(VERSION)-linux-x86_64
	cp cTetris cTetris-$(VERSION)-linux-x86_64/
	cp cTetris.desktop cTetris-$(VERSION)-linux-x86_64/
	cp cTetris.svg cTetris-$(VERSION)-linux-x86_64/
	cp LICENSE cTetris-$(VERSION)-linux-x86_64/
	cp README.md cTetris-$(VERSION)-linux-x86_64/
	tar -czf cTetris-$(VERSION)-linux-x86_64.tar.gz cTetris-$(VERSION)-linux-x86_64
	rm -rf cTetris-$(VERSION)-linux-x86_64
	@echo "Created cTetris-$(VERSION)-linux-x86_64.tar.gz"
else ifeq ($(PACKAGE),windows)
	$(MAKE) clean
	$(MAKE) windows DEBUG=
	rm -rf cTetris-$(VERSION)-windows-x86_64
	mkdir -p cTetris-$(VERSION)-windows-x86_64
	cp cTetris.exe cTetris-$(VERSION)-windows-x86_64/
	cp LICENSE cTetris-$(VERSION)-windows-x86_64/
	cp README.md cTetris-$(VERSION)-windows-x86_64/
	cd cTetris-$(VERSION)-windows-x86_64 && zip -r ../cTetris-$(VERSION)-windows-x86_64.zip . && cd ..
	rm -rf cTetris-$(VERSION)-windows-x86_64
	@echo "Created cTetris-$(VERSION)-windows-x86_64.zip"
else
	@echo "Error: PACKAGE must be 'linux' or 'windows'"
	@exit 1
endif

clean:
	rm -rf build
	rm -f cTetris cTetris_debug cTetris.exe cTetris_debug.exe
	rm -f cTetris-$(VERSION)-linux-x86_64.tar.gz
	rm -f cTetris-$(VERSION)-windows-x86_64.zip

.PHONY: help linux windows package clean package
