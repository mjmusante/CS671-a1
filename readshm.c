/*
 * Author: Mark Musante
 * Class: BU MET CS671, Spring 2015
 */

#define	_SVID_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int
main(int argc, char *argv[])
{
	key_t shmkey;
	int shmid;
	void *addr;

	if (argc != 3)
		return (1);

	if ((shmkey = ftok(argv[1], atoi(argv[2]))) == (key_t)-1) {
		perror("could not generate shm key");
		exit(1);
	}

	if ((shmid = shmget(shmkey, 0, 0666)) == -1) {
		perror("could not get shmid");
		exit(1);
	}

	if ((addr = shmat(shmid, NULL, 0)) == (void *)-1) {
		perror("could not attach to shared memory");
		exit(1);
	}

	printf("%p: contains %d entries\n", addr, *(int*)addr);

	return (0);
}
