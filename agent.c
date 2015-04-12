/*
 * Author: Mark Musante
 * Class: BU MET CS671, Spring 2015
 */

#define	_SVID_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define	NUM_COLLECTIONS 15
#define	INTERVAL 5

static void *
get_shm_addr(char *name, int id)
{
	key_t shmkey;
	int shmid;
	void *addr;

	if ((shmkey = ftok(name, id)) = (key_t)-1) {
		perror("could not generate shm key");
		exit(1);
	}

	if ((shmid = shmget(shmkey, 0, 0666)) == -1) {
		perror("could not get shmid");
		exit(1);
	}

	return (shmat(shmid, NULL, 0));
}

int
main(int argc, char *argv[])
{
	key_t shmkey;
	int shmid, fd;
	void *addr[NUM_COLLECTIONS];
	char *temp = tempnam("/tmp", ".collector");

	if (temp == NULL) {
		perror("could not generate temp file name");
		exit(1);
	}

	if ((fd = open(temp, O_CREAT | O_EXCL, 0666)) < 0) {
		perror("could not create temp file");
		exit(1);
	}

	for (int i = 0; i < NUM_COLLECTIONS; i++) {
		char syscmd[1024];

		sprintf(syscmd, "./perfmon %s %d", temp, i + 1);
		printf("> %s\n", syscmd);
		system(syscmd);
		addr[i] = get_shm_addr(temp, i + 1);
		sleep(INTERVAL);
	}

	return (0);
}
