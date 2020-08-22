STTARGET=libpthandlers.a
SHTARGET=libpthandlers.so
OBJTARGET=pthandlers.o
LIB=lib
CC=gcc
CFLAGS=-Wpedantic -Wall -g -pthread -foptimize-sibling-calls -O3
LFLAGS=-L$(LIB) -l:$(STTARGET)

.PHONY: all
all: lib examples

.PHONY: lib
lib: static shared

.PHONY: static
static: $(LIB)/$(STTARGET)

.PHONY: shared
shared: $(LIB)/$(SHTARGET)

$(LIB)/$(STTARGET): $(LIB)/$(OBJTARGET)
	ar rcs $(LIB)/$(STTARGET) $(LIB)/$(OBJTARGET)

$(LIB)/$(SHTARGET): $(LIB)/$(OBJTARGET)
	$(CC) -shared $(LIB)/$(OBJTARGET) -o $(LIB)/$(SHTARGET)

$(LIB)/pthandlers.o: $(LIB)/pthandlers.h $(LIB)/pthandlers.c
	$(CC) $(CFLAGS) -fPIC -c $(LIB)/pthandlers.c -o $(LIB)/$(OBJTARGET)

.PHONY: examples
examples: examples/state examples/dobedobe examples/divzero examples/coop

examples/state: static $(LIB)/pthandlers.h examples/state.c
	$(CC) $(CFLAGS) examples/state.c -o state $(LFLAGS)

examples/dobedobe: static $(LIB)/pthandlers.h examples/dobedobe.c
	$(CC) $(CFLAGS) examples/dobedobe.c -o dobedobe $(LFLAGS)

examples/divzero: static $(LIB)/pthandlers.h examples/divzero.c
	$(CC) $(CFLAGS) examples/divzero.c -o divzero $(LFLAGS)

examples/coop: static $(LIB)/pthandlers.h examples/coop.c
	$(CC) $(CFLAGS) examples/coop.c -o coop $(LFLAGS)


.PHONY: clean
clean:
	rm -f *.o
	rm -f lib/*.o lib/*.a lib/*.so
	rm -f examples/*.o
	rm -f state dobedobe
