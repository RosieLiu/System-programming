#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>

#include "potato.h"

void unlinkfifos(int players);

int main(int argc, char *argv[])
{
	if(argc < 3){
		printf("Error: Not enough parameters!\n");
		exit(0);
	}

	int players = atoi(argv[1]);
	int hops = atoi(argv[2]);

	if(players < 3){
		printf("Error: Not enough players!\n");
		exit(0);
	}else if(hops < 0 || hops > 512){
		printf("Error: Hops is negative or too large!\n");
		exit(0);
	}

	pid_t pid = getpid();
	srand((unsigned int)time(NULL));

	printf("Potato Ringmaster\n");
	printf("Players = %d\n", players);
	printf("Hops = %d\n", hops);

	int random = rand() % players;

	int fd_outs[players];
	int fd_ins[players];

	int i;

	for(i = 0; i < players; i++){
		char path1[50];
		char path2[50];
		sprintf(path1, "%smaster_p%d", LOC, i);
		sprintf(path2, "%sp%d_master", LOC, i);
		mkfifo(path1, 0666);
		mkfifo(path2, 0666);
	}

	for(i = 0; i < players; i++){
		char path[50];
		sprintf(path, "%smaster_p%d", LOC, i);
		fd_outs[i] = open(path, O_WRONLY);

	}

	for(i = 0; i < players; i++){
		char path[50];
		sprintf(path, "%sp%d_master", LOC, i);
		fd_ins[i] = open(path, O_RDONLY);
		while(fd_ins[i] == -1)
			fd_ins[i] = open(path, O_RDONLY);
	}


	for(i = 0; i < players; i++){

		write(fd_outs[i], &players, sizeof(int));
	}

	for(i = 0; i < players; i++){
		int buf;
		read(fd_ins[i], &buf, sizeof(buf));
		printf("Player %d is ready to play\n", buf);
	}


	fd_set rfds;
	FD_ZERO(&rfds);
	int nfds = -1;
	for(i = 0; i < players; i++){
		FD_SET(fd_ins[i], &rfds);
		nfds = nfds >= fd_ins[i] ? nfds : fd_ins[i];
	}
	nfds += 1;

	printf("All players present, sending potato to player %d\n", random);
	potato_t *potato = malloc(sizeof(potato_t));
	potato->msgType = GO;
	potato->totalHops = hops;
	potato->hopCount = hops;

	write(fd_outs[random], (void *)potato, sizeof(potato_t));
	while(1){
		int fds;
		fd_set c_rfds = rfds;
		fds = select(nfds, &c_rfds, NULL, NULL, NULL);
		if(fds > 0){
			int revfd = -1;
			memset((void *)potato, 0, sizeof(potato_t));
			for(i = 0; i < players; i++)
				if(FD_ISSET(fd_ins[i], &c_rfds)){
					revfd = fd_ins[i];
					break;
				}
			if(revfd == -1) continue;

			memset(potato, 0, sizeof(potato_t));
			int size;
			size = read(revfd, (void *)potato, sizeof(potato_t));
	

			if(potato->msgType == OVER){
				for(i = 0; i < players; i++){
					write(fd_outs[i], (void *)potato, sizeof(potato_t));
				}
				printf("Trace of potato:\n");
				printf("%lu", potato->hop_trace[0]);
				for(i = 1; i < potato->totalHops; i++){
					printf(",%ld", potato->hop_trace[i]);
				}
				printf("\n");
				break;
			}
		}
	}



	sleep(1);
	for(i = 0; i < players; i++){
		close(fd_ins[i]);
		close(fd_outs[i]);
	}

	unlinkfifos(players);

	exit(0);
  
}

void unlinkfifos(int players){
	int i;
	for(i = 0; i < players; i++){
		char mas_to_player[50];
		char player_to_mas[50];
		sprintf(mas_to_player, "%smaster_p%d", LOC, i);
		sprintf(player_to_mas, "%sp%d_master", LOC, i);
		unlink(mas_to_player);
		unlink(player_to_mas);
	}

	for(i = 0; i < players; i++){
		char toright[50];
		char fromright[50];
		if(i != players - 1){
			sprintf(toright, "%sp%d_p%d", LOC, i, i+1);
			sprintf(fromright, "%sp%d_p%d", LOC, i+1, i);
		}else{
			sprintf(toright, "%sp%d_p%d", LOC, i, 0);
			sprintf(fromright, "%sp%d_p%d", LOC, 0, i);
		}

		unlink(toright);
		unlink(fromright); 
	}
}
