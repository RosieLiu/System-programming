#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>


#include "potato.h"


int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("%d\n", argc);
		printf("usage: rmfifo num_of_players.\n");
	}
	int numPlayers = atoi(argv[1]);

	char fifoName[100] = "";
	int i = 0;
	for (i = 0; i < numPlayers; i++) {
		sprintf(fifoName, "%smaster_p%d", LOC, i);
		unlink(fifoName);
		sprintf(fifoName, "%sp%d_master", LOC, i);
		unlink(fifoName);
		sprintf(fifoName, "%sp%d_p%d", LOC, i, (i+1)%numPlayers);
		unlink(fifoName);
		sprintf(fifoName, "%sp%d_p%d", LOC, i, (i-1+numPlayers)%numPlayers);
		unlink(fifoName);
	}
 	exit(0);
}

