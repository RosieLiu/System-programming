#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "potato.h"


int main(int argc, char *argv[])
{
	int i = 0;
	for (i = 0; i < argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");

	if (argc != 3) {
		printf("%d\n", argc);
		printf("usage: ringmaster number_of_players number_of_hops\n");
	}
	int numPlayers = atoi(argv[1]);
	int numHops = atoi(argv[2]);
	printf("Potato Ringmaster \nPlayers = %d \nHops = %d\n", numPlayers, numHops);

	
	/* Make FIFOs */
	char fifoName[BUFF_LEN] = "";

	for (i = 0; i < numPlayers; i++) {
		clearString(fifoName);
		sprintf(fifoName, "%smaster_p%d", LOC, i);
		if (0 != mkfifo(fifoName, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) {
			printf("make fifo %s failed\n", fifoName);
		}
		clearString(fifoName);
		sprintf(fifoName, "%sp%d_master", LOC, i);
		if (0 != mkfifo(fifoName, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) {
			printf("make fifo %s failed\n", fifoName);
		}
		clearString(fifoName);
		sprintf(fifoName, "%sp%d_p%d", LOC, i, (i-1+numPlayers) % numPlayers);
		if (0 != mkfifo(fifoName, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) {
			printf("make fifo %s failed\n", fifoName);
		}
		clearString(fifoName);
		sprintf(fifoName, "%sp%d_p%d", LOC, i, (i+1) % numPlayers);
		if (0 != mkfifo(fifoName, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) {
			printf("make fifo %s failed\n", fifoName);
		}
	}

 	char buf[BUFF_LEN];
 	ssize_t len = 0;

	fd_set setRD, setWR, setCur;
 	FD_ZERO(&setRD);
 	FD_ZERO(&setWR);
 	int setSize = 0;
	
	int *fdWR = malloc(numPlayers * sizeof(int));
	int *fdRD = malloc(numPlayers * sizeof(int));

	/* Open FIFOs between master and players */
	for (i = 0; i < numPlayers; i++) {
		fdWR[i] = fdRD[i] = 0;

		clearString(fifoName);
		sprintf(fifoName, "%smaster_p%d", LOC, i);
		fdWR[i] = open(fifoName, O_WRONLY);
		// printf("master write %d to player %d\n", fdWR[i], i);

		/* Broadcast number of players */
		clearString(buf);
		sprintf(buf, "%d", numPlayers);
		len = write(fdWR[i], buf, BUFF_LEN);
		assert(len == BUFF_LEN);
		
		clearString(fifoName);
		sprintf(fifoName, "%sp%d_master", LOC, i);
		fdRD[i] = open(fifoName, O_RDONLY);

		clearString(buf);
		len = read(fdRD[i], buf, BUFF_LEN);
		assert(strcmp(buf, "ready") == 0);
		printf("Player %d is ready to play\n", i);

	 	FD_SET(fdRD[i], &setRD);
	 	FD_SET(fdWR[i], &setWR);
	}

	/* Create a new potato */
 	potato_t realPotato;
 	potato_t *potato = &realPotato;
 	potato->msgType = GO;
 	potato->totalHops = numHops;
 	potato->hopCount = numHops;

	/* Start game: randomly choose a player */
 	srand((unsigned int) time(NULL));
	int random = rand() % numPlayers;
	printf("Send a potato with %d hops\n", potato->hopCount);
	len = write(fdWR[2], (void *)potato, sizeof(potato_t));
	// len = write(fdWR[random], (void *)potato, sizeof(potato_t));
	assert(len == sizeof(potato_t));
	printf("All players present, sending potato to player %d\n", random);


	/* Clear FIFOs */
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

