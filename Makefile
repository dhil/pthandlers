CC=gcc
CFLAGS=-Wpedantic -Wall -g -pthread -foptimize-sibling-calls -O1
LFLAGS=-Llib -l:libpthandlers.a
TARGET=demo

.PHONY: all
all: lib examples

.PHONY: lib
lib: lib/libpthandlers.a lib/libpthandlers.so

.PHONY: examples
examples: examples/state examples/dobedobe

lib/pthandlers.o: lib/pthandlers.h lib/pthandlers.c
	$(CC) $(CFLAGS) -fPIC -c lib/pthandlers.c -o lib/pthandlers.o

lib/libpthandlers.a: lib/pthandlers.o
	ar rcs lib/libpthandlers.a lib/pthandlers.o

lib/libpthandlers.so: lib/pthandlers.o
	$(CC) -shared lib/pthandlers.o -o lib/libpthandlers.so

examples/state: lib/libpthandlers.a lib/pthandlers.h examples/state.c
	$(CC) $(CFLAGS) examples/state.c -o state $(LFLAGS)

examples/dobedobe: lib/libpthandlers.a lib/pthandlers.h examples/dobedobe.c
	$(CC) $(CFLAGS) examples/dobedobe.c -o dobedobe $(LFLAGS)


.PHONY: clean
clean:
	rm -f $(TARGET)
	rm -f *.o
	rm -f lib/*.o lib/*.a lib/*.so
	rm -f examples/*.o
	rm -f state dobedobe
