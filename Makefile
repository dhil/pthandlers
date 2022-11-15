STTARGET=libptfx.a
SHTARGET=libptfx.so
OBJTARGET=ptfx.o
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

$(LIB)/ptfx.o: $(LIB)/ptfx.h $(LIB)/ptfx.c
	$(CC) $(CFLAGS) -fPIC -c $(LIB)/ptfx.c -o $(LIB)/$(OBJTARGET)

.PHONY: examples

examples: examples/test examples/state examples/pipes examples/coop
#examples/state examples/dobedobe examples/divzero examples/coop examples/ovlreader

examples/state: static $(LIB)/ptfx.h examples/state.c
	$(CC) $(CFLAGS) examples/state.c -o state $(LFLAGS)

examples/dobedobe: static $(LIB)/ptfx.h examples/dobedobe.c
	$(CC) $(CFLAGS) examples/dobedobe.c -o dobedobe $(LFLAGS)

examples/divzero: static $(LIB)/ptfx.h examples/divzero.c
	$(CC) $(CFLAGS) examples/divzero.c -o divzero $(LFLAGS)

examples/coop: static $(LIB)/ptfx.h examples/coop.c
	$(CC) $(CFLAGS) examples/coop.c -o coop $(LFLAGS)

examples/ovlreader: static $(LIB)/ptfx.h examples/ovlreader.c
	$(CC) $(CFLAGS) examples/ovlreader.c -o ovlreader $(LFLAGS)

examples/generator: static $(LIB)/ptfx.h examples/generator.c
	$(CC) $(CFLAGS) examples/generator.c -o generator $(LFLAGS)

examples/test: static $(LIB)/ptfx.h examples/test.c
	$(CC) $(CFLAGS) examples/test.c -o test $(LFLAGS)

examples/pipes: static $(LIB)/ptfx.h examples/pipes.c
	$(CC) $(CFLAGS) examples/pipes.c -o pipes $(LFLAGS)


.PHONY: clean
clean:
	rm -f *.o
	rm -f lib/*.o lib/*.a lib/*.so
	rm -f examples/*.o
	rm -f state dobedobe generator coop divzero ovlreader test
