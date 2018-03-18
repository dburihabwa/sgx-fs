TARGET = fusegx
CC ?= gcc
CFLAGS = -Wextra -Wall -pedantic -c
CFLAGS += $(shell pkg-config fuse --cflags)
INCLUDE = $(shell pkg-config fuse --libs)

OBJECTS=fusegx.o

all: $(TARGET)
	
$(TARGET): $(OBJECTS)
	$(CC) $< $(INCLUDE) -o $(TARGET)

fusegx.o: src/fusegx.c
	$(CC) $< $(CFLAGS) -o $@

clean:
	rm -f $(OBJECTS)

realclean: clean
	rm -f $(TARGET)
