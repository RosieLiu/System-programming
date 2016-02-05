CC=gcc
CFLAGS=-O3 -fPIC
MALLOC_VERSION=FF
WDIR=~/Desktop/ece650/
LDFLAGS = -shared
RM = rm -f
SRCS = my_malloc.c
OBJS = $(SRCS:.c=.o)
HEADS = $(SRCS:.c=.h)
LD = ld

all: mymalloc_test
#all: my_malloc.o
#all: libmymalloc_SO

mymalloc_test: mymalloc_test.c libmymalloc_SO 
	$(CC) $(CFLAGS) -I$(WDIR) -L$(WDIR) -D$(MALLOC_VERSION) -Wl,-rpath=$(WDIR) -o $@ mymalloc_test.c -lmymalloc -lrt

### link object file  
libmymalloc_SO: $(OBJS) 
	$(LD) -fPIC $(LDFLAGS) -o -g libmymalloc.so $^   
	sudo rm /usr/lib/libmymalloc.so
	sudo ln -s $(WDIR)/libmymalloc.so /usr/lib/libmymalloc.so
      
### compile source code
my_malloc.o: $(SRCS) $(HEADS) 
	gcc -c -g -fPIC $< 

clean:
	rm -f *~ *.so *.o mymalloc_test

clobber:
	rm -f *~ *.o 
