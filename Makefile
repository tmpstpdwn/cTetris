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
	@echo "make what???"
	@echo ""
	@echo "make linux              - Linux release build"
	@echo "make linux DEBUG=1      - Linux debug build"
	@echo "make windows            - Windows release build"
	@echo "make windows DEBUG=1    - Windows debug build"
	@echo "make package            - Package Linux release build"
	@echo "make clean              - Remove build artifacts"

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

package:
	$(MAKE) clean
	$(MAKE) linux DEBUG=
	rm -rf cTetris-linux-x86_64
	mkdir -p cTetris-linux-x86_64
	cp cTetris cTetris-linux-x86_64/
	cp cTetris.desktop cTetris-linux-x86_64/
	cp cTetris.svg cTetris-linux-x86_64/
	tar -czf cTetris-linux-x86_64.tar.gz cTetris-linux-x86_64
	rm -rf cTetris-linux-x86_64
	@echo "Created cTetris-linux-x86_64.tar.gz"

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

clean:
	rm -rf build
	rm -f cTetris cTetris_debug cTetris.exe cTetris_debug.exe
	rm -f cTetris-linux-x86_64.tar.gz

.PHONY: help linux windows package clean
