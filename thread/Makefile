CC=gcc
CFLAGS=-O3 -fPIC

MALLOC_VERSION=BF
WDIR=/home/ubuntu/Desktop/ece650/heap/thread/



all: thread_test thread_test_malloc_free
# all: thread_test_malloc_free
# all: libmymalloc.so

thread_test: thread_test.c libmymalloc.so
	$(CC) $(CFLAGS) -I$(WDIR) -L$(WDIR) -Wl,-rpath=$(WDIR) -g -o $@ $^ -lmymalloc -lpthread

thread_test_malloc_free: thread_test_malloc_free.c libmymalloc.so
	$(CC) $(CFLAGS) -I$(WDIR) -L$(WDIR) -Wl,-rpath=$(WDIR) -g -o $@ $^ -lmymalloc -lpthread

libmymalloc.so: my_malloc.o
	ld -shared -fPIC -L$(WDIR)  -o $@ $^

my_malloc.o: my_malloc.c my_malloc.h
	$(CC) $(CFLAGS)  -g -c  $< -lpthread

clean:
	rm -f *~ *.o *.so 

clobber:
	rm -f *~ *.o