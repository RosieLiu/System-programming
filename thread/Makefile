CC=gcc
CFLAGS= -fPIC

MALLOC_VERSION=BF
WDIR=~/Desktop/ece650/heap/thread/



all: thread_test
# all: ts_malloc.o

thread_test: thread_test.c libmymalloc.so
	$(CC) $(CFLAGS) -I$(WDIR) -L$(WDIR) -Wl,-rpath=$(WDIR) -g -o $@ $< -lmymalloc -lpthread


libmymalloc.so: my_malloc.o
	ld -shared -fPIC -L$(WDIR)  -o $@ $^

my_malloc.o: my_malloc.c my_malloc.h
	$(CC) $(CFLAGS)  -g -c  $< -lpthread

clean:
	rm -f *~ *.o *.so 

clobber:
	rm -f *~ *.o