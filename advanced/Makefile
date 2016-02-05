CC=gcc
#CFLAGS=-O3 -fPIC
CFLAGS=-fPIC
MALLOC_VERSION=FF
WDIR=~/Desktop/ece650/heap/advanced
LDFLAGS = -shared
RM = rm -f
SRCS = my_malloc.c
OBJS = $(SRCS:.c=.o)
HEADS = $(SRCS:.c=.h)
LD = ld
SLIB = libmymalloc.so


all: equal_size_allocs small_range_rand_allocs large_range_rand_allocs

#all: libmymalloc_SO

equal_size_allocs: equal_size_allocs.c libmymalloc.so
	$(CC) $(CFLAGS) -I$(WDIR) -L$(WDIR) -D$(MALLOC_VERSION) -Wl,-rpath=$(WDIR) -g -o $@ equal_size_allocs.c -lmymalloc -lrt

small_range_rand_allocs: small_range_rand_allocs.c libmymalloc.so
	$(CC) $(CFLAGS) -I$(WDIR) -L$(WDIR) -D$(MALLOC_VERSION) -Wl,-rpath=$(WDIR) -g -o $@ small_range_rand_allocs.c -lmymalloc -lrt

large_range_rand_allocs: large_range_rand_allocs.c libmymalloc.so
	$(CC) $(CFLAGS) -I$(WDIR) -L$(WDIR) -D$(MALLOC_VERSION) -Wl,-rpath=$(WDIR) -g -o $@ large_range_rand_allocs.c -lmymalloc -lrt

### link object file  
libmymalloc.so: $(OBJS) 
	$(LD) -fPIC $(LDFLAGS) -o  $(SLIB) $^   
	# sudo rm /usr/lib/$(SLIB)
	# sudo ln -s $(WDIR)/$(SLIB) /usr/lib/$(SLIB)
      
### compile source code
my_malloc.o: $(SRCS) $(HEADS) 
	gcc -g -c -fPIC $< 



clean:
	rm -f *.so *~ *.o equal_size_allocs small_range_rand_allocs large_range_rand_allocs

clobber:
	rm -f *~ *.o
