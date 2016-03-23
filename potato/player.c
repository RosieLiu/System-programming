#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <assert.h>

#include "potato.h"


int main(int argc, char *argv[])
{
	int numPlayers = 0;
	int idx = atoi(argv[1]);

	/* Open the channel between player and master */
	char fifoName[BUFF_LEN] = "";
	
	clearString(fifoName);
	sprintf(fifoName, "%smaster_p%d", LOC, idx);
 	int masterRD = open(fifoName, O_RDONLY);
	
	clearString(fifoName);
	sprintf(fifoName, "%sp%d_master", LOC, idx);
 	int masterWR = open(fifoName, O_WRONLY);
 	
 	/* Initialize FD sets */
 	fd_set setRD, setWR;
 	FD_ZERO(&setRD);
	
 	char buf[BUFF_LEN] ;
 	ssize_t len = 0;

 	fd_set setCur;
 	int setSize = 0;

	/* Read number of players */
	clearString(buf);
	len = read(masterRD, buf, BUFF_LEN);
	numPlayers = atoi(buf);
 	printf("--- Connected as player %d out of %d total players\n", idx, numPlayers);
 	clearString(buf);
 	sprintf(buf, "ready");
 	len = write(masterWR, buf, BUFF_LEN);

	/* Initialize relevant FDs */
	int leftRD = -1, rightRD = -1, leftWR = -1, rightWR = -1;
	if (idx == numPlayers - 1) {
	 	clearString(fifoName);
	 	sprintf(fifoName, "%sp%d_p%d", LOC, idx, (idx-1+numPlayers)%numPlayers);
	 	leftWR = open(fifoName, O_WRONLY);

	 	clearString(fifoName);
	 	sprintf(fifoName, "%sp%d_p%d", LOC, (idx+1)%numPlayers, idx);
	 	rightRD = open(fifoName, O_RDONLY);

	 	clearString(fifoName);
	 	sprintf(fifoName, "%sp%d_p%d", LOC, idx, (idx+1)%numPlayers);
	 	rightWR = open(fifoName, O_WRONLY);

	 	clearString(fifoName);
	 	sprintf(fifoName, "%sp%d_p%d", LOC, (idx-1+numPlayers)%numPlayers, idx);
	 	leftRD = open(fifoName, O_RDONLY);
 	}
 	else {
	 	clearString(fifoName);
	 	sprintf(fifoName, "%sp%d_p%d", LOC, (idx+1)%numPlayers, idx);
	 	rightRD = open(fifoName, O_RDONLY);

	 	clearString(fifoName);
	 	sprintf(fifoName, "%sp%d_p%d", LOC, idx, (idx-1+numPlayers)%numPlayers);
	 	leftWR = open(fifoName, O_WRONLY);

	 	clearString(fifoName);
	 	sprintf(fifoName, "%sp%d_p%d", LOC, (idx-1+numPlayers)%numPlayers, idx);
	 	leftRD = open(fifoName, O_RDONLY);

	 	clearString(fifoName);
	 	sprintf(fifoName, "%sp%d_p%d", LOC, idx, (idx+1)%numPlayers);
	 	rightWR = open(fifoName, O_WRONLY);
 	}

 	printf("--- No deadlock\n");
 	
 	FD_SET(leftRD, &setRD);
 	FD_SET(rightRD, &setRD);
 	FD_SET(masterRD, &setRD);

 	FD_SET(leftWR, &setWR);
 	FD_SET(rightWR, &setWR);
 	FD_SET(masterWR, &setWR);
	
	/* Get potato from master or neighbor players */
 	potato_t realPotato;
 	potato_t *potato = &realPotato;
 	// potato_t *potato = malloc(sizeof(potato_t));
 	// memset((void *)potato, 0, sizeof(potato_t));

 	// FD_COPY(&setRD, &setCur);
 	setCur = setRD;
	printf("--- player %d is goint to read.\n", idx);
 	if ((setSize = select(FD_SETSIZE, &setCur, NULL, NULL, NULL)) > 0) {
 		
 		assert(setSize == 1);

 		/* Get potato */
 		int potatoFD = -1;
 		if (FD_ISSET(masterRD, &setCur)) {
 			potatoFD = masterRD;
	 		printf("--- Player %d read master\n", idx);
 		}
 		if (FD_ISSET(leftRD, &setCur)) {
 			potatoFD = leftRD;
	 		printf("--- Player %d read left\n", idx);
 		}
 		if (FD_ISSET(rightRD, &setCur)) {
 			potatoFD = rightRD;
	 		printf("--- Player %d read right\n", idx);
 		}

 		if (potatoFD != -1) {
	 		clearString(buf);
	 		len = read(potatoFD, (void *)potato, sizeof(potato_t));
	 		printf("--- Player %d read len = %zu \nget a potato with %d hops\n", idx, len, potato->hopCount);
	 		potato->hopCount--;
 		}
 	}

 	close(masterRD);
 	close(masterWR);
 	exit(0);
}
