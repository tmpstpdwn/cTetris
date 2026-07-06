CC = gcc
CFLAGS = -Iinclude
LDFLAGS = -Llibs -lraylib
LDLIBS = -lm -lpthread -ldl -lrt

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
OUT = cTetris

.DEFAULT_GOAL := help

help:
	@echo "make what???"
	@echo ""
	@echo "make linux      - Compile native Linux binary"
	@echo "make windows    - Cross-compile Windows executable (.exe)"
	@echo "make install    - Install cTetris to local paths (linux)"
	@echo "make uninstall  - Uninstall cTetris from local paths (linux)"
	@echo "make clean      - Remove build artifacts"

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

linux: $(OBJ)
	$(CC) $(OBJ) -o $(OUT) $(LDFLAGS) $(LDLIBS)

windows: CC = x86_64-w64-mingw32-gcc
windows: OUT = cTetris.exe
windows: CFLAGS = -Iinclude
windows: LDFLAGS = -Llib_win -lraylib
windows: LDLIBS = -lopengl32 -lgdi32 -lwinmm -lws2_32 -Wl,--defsym=stat64i32=_stat64i32 -mwindows
windows: $(OBJ) resource.o
	$(CC) $(OBJ) resource.o -o $(OUT) $(LDFLAGS) $(LDLIBS)

resource.o: resource.rc
	x86_64-w64-mingw32-windres resource.rc -O coff -o resource.o

install: linux
	@echo "Installing cTetris..."
	@mkdir -p ~/.local/bin
	@cp cTetris ~/.local/bin/
	@echo "Installed binary at ~/.local/bin"

	@mkdir -p ~/.local/share/icons/hicolor/256x256/apps
	@cp cTetris_256.png ~/.local/share/icons/hicolor/256x256/apps/cTetris.png
	@echo "Installed icon at ~/.local/share/icons/hicolor/256x256/apps"

	@mkdir -p ~/.local/share/applications
	@cp cTetris.desktop ~/.local/share/applications/
	@echo "Installed desktop entry at ~/.local/share/applications"

	@printf "\033[31mNOTE:\033[0m Ensure ~/.local/bin is in your PATH.\n"
	@echo "Installation successful! Run 'cTetris'."

uninstall:
	@echo "Uninstalling cTetris..."
	rm -f ~/.local/bin/cTetris
	@echo "Uninstalled binary from ~/.local/bin"

	rm -f ~/.local/share/icons/hicolor/256x256/apps/cTetris.png
	@echo "Uninstalled icon from ~/.local/share/icons/hicolor/256x256/apps"

	rm -f ~/.local/share/applications/cTetris.desktop
	@echo "Uninstalled desktop entry from ~/.local/share/applications"

	@echo "Uninstall complete."

clean:
	rm -f cTetris cTetris.exe resource.o $(OBJ)

.PHONY: help linux windows install uninstall clean
