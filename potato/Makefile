CC = gcc
CFLAGS = -g

all: ringmaster player rmfifo

ringmaster: ringmaster.o
	$(CC) $(CFLAGS) -o $@ $^

player: player.o
	$(CC) $(CFLAGS) -o $@ $^

rmfifo: rmfifo.o
	$(CC) $(CFLAGS) -o $@ $^


ringmaster.o: ringmaster.c potato.h
	$(CC) -c $^

player.o: player.c potato.h
	$(CC) -c $^

rmfifo.o: rmfifo.c potato.h
	$(CC) -c $^

clobber:
	rm -f *.o *~

clean:
	rm -rf *.o ringmaster player *~
