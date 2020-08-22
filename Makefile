CC=gcc
CFLAGS=-Wpedantic -Wall -g -pthread -foptimize-sibling-calls -O1
TARGET=demo

.PHONY: all
all: lib

.PHONY: lib
lib: pthreadhandlers.h pthreadhandlers.c
	$(CC) $(CFLAGS) pthreadhandlers.c -o $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)
	rm -f *.o
