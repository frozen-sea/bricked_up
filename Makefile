CC=gcc
CFLAGS+=$(shell pkg-config --cflags sdl3 sdl3-ttf)
LDFLAGS+=$(shell pkg-config --libs sdl3 sdl3-ttf) -lm
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
