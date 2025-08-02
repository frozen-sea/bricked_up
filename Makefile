CC=gcc
CFLAGS=-I/usr/include/SDL2 -D_REENTRANT
LDFLAGS=-lSDL2 -lSDL2_ttf -lm
TARGET=bricked_up
SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
