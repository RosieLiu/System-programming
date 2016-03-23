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
	len = read(masterRD, &numPlayers, sizeof(int));
 	clearString(buf);
 	sprintf(buf, "ready");
 	len = write(masterWR, buf, BUFF_LEN);
 	printf("--- Connected as player %d out of %d total players\n", idx, numPlayers);

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

 	FD_SET(leftRD, &setRD);
 	FD_SET(rightRD, &setRD);
 	FD_SET(masterRD, &setRD);

 	FD_SET(leftWR, &setWR);
 	FD_SET(rightWR, &setWR);
 	FD_SET(masterWR, &setWR);
	
	/* Get potato from master or neighbor players */
 	potato_t realPotato;
 	potato_t *potato = &realPotato;
 	setCur = setRD;
 	srand((unsigned int) time(NULL));
 	while ((setSize = select(FD_SETSIZE, &setCur, NULL, NULL, NULL)) > 0) {
 		
 		printf("--- player %d\n", idx);

 		if (setSize != 1) {
 			printf("--- set size = %d\n", setSize);
 		}

 		/* Get message */
 		int potatoFD = -1;
 		if (FD_ISSET(masterRD, &setCur)) {
 			potatoFD = masterRD;
 			len = read(potatoFD, (void *)potato, sizeof(potato_t));
 			printf("--- from master read %zu\n", len);
 		}
 		else if (FD_ISSET(leftRD, &setCur)) {
 			potatoFD = leftRD;
 			len = read(potatoFD, (void *)potato, sizeof(potato_t));
 			printf("--- from left read %zu\n", len);
 		}
 		else if (FD_ISSET(rightRD, &setCur)) {
 			potatoFD = rightRD;
 			len = read(potatoFD, (void *)potato, sizeof(potato_t));
 			printf("--- from right read %zu\n", len);
 		}

 		if (potatoFD == -1) {
 			printf("--- continue\n");
 			continue;
 		}


		/* Exit */

		if (potato->msgType == GO){
	 		/* Get potato */
	 		printf("--- Player %d gets a potato with %d hops\n", idx, potato->hopCount);
	 		potato->hop_trace[potato->totalHops - potato->hopCount] = idx;
	 		potato->hopCount--;
	
	 		/* Transfer potato */
	 		if (potato->hopCount != 0) {
				int ran = rand() % 2;
				int nextFD = -1;
				printf("--- random = %d\n", ran);
				if (ran == 0) {
					nextFD = leftWR;
					printf("--- Sending potato to %d\n", (idx-1+numPlayers)%numPlayers);
				}
				else if (ran == 1) {
					nextFD = rightWR;
					printf("--- Sending potato to %d\n", (idx+1)%numPlayers);
				}
				write(nextFD, (void *)potato, sizeof(potato_t));
	 		}
	 		/* I'm it */
	 		else {
	 			potato->msgType = OVER;
	 			printf("--- I'm it player %d\n", idx);
	 			len = write(masterWR, (void *)potato, sizeof(potato_t));
	 			assert(len == sizeof(potato_t));
	
	 		}
	 	}
	 	else if (potato->msgType == OVER) {
	 		printf("--- Player %d exit itself\n", idx);

	 		sprintf(fifoName, "%smaster_p%d", LOC, idx);
	 		close(masterRD);
			unlink(fifoName);

			sprintf(fifoName, "%sp%d_master", LOC, idx);
			close(masterWR);
			unlink(fifoName);

			close(leftRD);
			close(leftWR);
			close(rightRD);
			close(rightWR);
	 		break;
	 	}
	 	else {
	 		printf("--- wrong %c\n", potato->msgType);
	 		break;
	 	}
 	}

 	close(masterRD);
 	close(masterWR);
 	exit(0);
}