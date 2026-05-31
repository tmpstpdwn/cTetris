CC = gcc
CFLAGS = -Iinclude -g -O0
LDFLAGS = -Llibs -lraylib
LDLIBS = -lm -lpthread -ldl -lrt

SRC = $(wildcard src/*.c)
OUT = tetris

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(SRC) -o $(OUT) $(CFLAGS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(OUT)
