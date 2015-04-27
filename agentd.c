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
#include <sys/socket.h>
#include <arpa/inet.h>

#include "procdata.h"


#define	NUM_COLLECTIONS 15		// how many times we should collect
#define	INTERVAL 2			// time (seconds) between collections
#define	SERVER_PORT	5678		// port to bind to

/*
 * This structure contains the summary data for a single pid across
 * all collections. This is generated once per pid found in the shared
 * memory.
 */
typedef struct summary {
	double start_utime;
	double end_utime;
	double start_stime;
	double end_stime;
	unsigned long long total_rss;
	unsigned long long total_data;
	unsigned long long total_shared;
	unsigned long long total_library;
	unsigned long total_majflt;
	unsigned long total_minflt;
	unsigned int count;
} summary_t;

/*
 * Each collection gets its own shm_data struct.
 */
typedef struct shm_data {
	void *addr;
	int shmid;
	int cur;
	int done;
} shm_data_t;

/*
 * This macro gives us a pointer to the start of the procdata_t structure
 * that the ".cur" value points to.
 */
#define	CURADDR(data) (&(((procdata_t *)(((char *)data.addr) + 8)) \
				[data.cur]))

/*
 * Function:	get_shm_addr
 * Description:	set the shmid and the addr values for the shared memory
 * Input:	name = filename for ftok
 * 		id = id for ftok
 * 		data = pointer to shm data to fill in
 * Returns:	void
 */
static void
get_shm_addr(char *name, int id, shm_data_t *data)
{
	key_t shmkey;

	if ((shmkey = ftok(name, id)) == (key_t)-1) {
		perror("could not generate shm key");
		exit(1);
	}

	if ((data->shmid = shmget(shmkey, 0, 0666)) == -1) {
		perror("could not get shmid");
		exit(1);
	}

	if ((data->addr = shmat(data->shmid, NULL, 0)) == (void *)-1) {
		perror("could not get shmaddr");
		exit(1);
	}
}

/*
 * Function:	init_lowest
 * Description:	Find the lowest pid in a given shared memory region
 * Input:	data = pointer to the shared memory data
 * Returns:	void
 */
static void
init_lowest(shm_data_t *data)
{
	procdata_t *pd = (procdata_t *)(((char *)data->addr) + 8);
	int max = *(int *)data->addr;
	
	for (int i = 0; i < max; i++) {
		if (pd[i].pid != 0) {
			data->cur = i;
			data->done = 0;
			return;
		}
	}
	data->done = 1;
}

/*
 * The following fields are summarised:
 * utime	- time: end - start
 * stime	- time: end - start
 * rss		- average: sum / count
 * data		- average: sum / count
 * shared	- average: sum / count
 * library	- average: sum / count
 * majflt	- average: sum / count
 * minflt	- average: sum / count
 *
 * The following fields are left out of the summary as they do not
 * vary over the course of the execution:
 *	pid, ppid, pgrp, [re][gu]id
 */
static void
do_summary(summary_t *sum, procdata_t *pd)
{
	if (sum->start_utime == 0)
		sum->start_utime = sum->end_utime = pd->utime;
	else
		sum->end_utime = pd->utime;

	if (sum->start_stime == 0)
		sum->start_stime = sum->end_stime = pd->stime;
	else
		sum->end_stime = pd->stime;

	sum->total_rss += pd->rss;
	sum->total_data += pd->data;
	sum->total_shared += pd->shared;
	sum->total_library += pd->library;
	sum->total_majflt += pd->majflt;
	sum->total_minflt += pd->minflt;

	sum->count++;
}

/*
 * Function:	scan_one
 * Description:	Scan the lowest pid, and create a summary of the data
 * Input:	shmdata = pointer to the shared memory data
 * 		fd = file descriptor to write to
 * Returns:	0 if there are no more pids to scan, 1 if there are
 */
static int
scan_one(shm_data_t *shmdata, int fd, int start)
{
	int found = 0;
	int lowest, len, i;
	summary_t sum;
	char buf[4096];

	i = start;
	do {
		if (shmdata[i].done) {
			i = (i + 1) % NUM_COLLECTIONS;
			continue;
		}

		procdata_t *pd = CURADDR(shmdata[i]);

		if (!found && pd->pid > 0) {
			lowest = pd->pid;
			found = 1;
		} else if (pd->pid > 0 && pd->pid < lowest) {
			lowest = pd->pid;
		}
		i = (i + 1) % NUM_COLLECTIONS;
	} while (i != start);

	if (!found) {
		/*
		 * No more pids were found. Clear out the 'done' values
		 * and return 0.
		 */
		for (int i = 0; i < NUM_COLLECTIONS; i++)
			shmdata[i].done = 0;
		return (0);
	}

	memset(&sum, 0, sizeof (summary_t));

	i = start;
	do {
		procdata_t *pd = CURADDR(shmdata[i]);
		int max = *(int *)shmdata[i].addr;

		if (pd->pid != lowest) {
			i = (i + 1) % NUM_COLLECTIONS;
			continue;
		}

		do_summary(&sum, pd);

		++shmdata[i].cur;
		if (shmdata[i].cur >= max)
			shmdata[i].done = 1;
		while (!shmdata[i].done) {
			pd = CURADDR(shmdata[i]);
			if (pd->pid == 0) {
				++shmdata[i].cur;
				if (shmdata[i].cur >= max)
					shmdata[i].done = 1;
			} else {
				break;
			}
		}
		i = (i + 1) % NUM_COLLECTIONS;
	} while (i != start);
	
	len = sprintf(buf, "%d, %f, %f, %llu, %llu, %llu, %llu, %lu, %lu\n",
	    lowest, sum.end_utime - sum.start_utime,
	    sum.end_stime - sum.start_stime,
	    sum.total_rss / sum.count,
	    sum.total_data / sum.count,
	    sum.total_shared / sum.count,
	    sum.total_library / sum.count,
	    sum.total_majflt / sum.count,
	    sum.total_minflt / sum.count);
	write(fd, buf, len);
	return (1);
}	

/*
 * Function:	open_port
 * Description:	Create a port and listen on it
 * Returns:	file descriptor of socket. Exits on error.
 */
int
open_port()
{
	int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	int on = 1;
	struct sockaddr_in addr = { 0 };

	if (fd < 0) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(fd, 0, SO_REUSEADDR, &on, sizeof (int)) < 0) {
		perror("setsockopt SO_REUSEADDR");
		exit(1);
	}

	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(fd, (struct sockaddr *)&addr, sizeof (addr)) < 0) {
		perror("bind");
		exit(1);
	}

	(void) listen(fd, 10);
	return (fd);
}

/*
 * Function:	serve_up
 * Description:	Serve data up to a client
 * Input:	shmdata = pointer to shared memory data
 * 		fd = file descriptor to write to
 * 		ready = is the 15-minute scan ready yet
 */
void
serve_up(shm_data_t *shmdata, int fd, int ready, int start)
{
	uint32_t len;

	/* first four bytes is the message length, in network order */
	if (read(fd, &len, sizeof (len)) < 0) {
		perror("read");
		return;
	}

	/* read the command */
	char *command = malloc(len);
	if (read(fd, command, len) < 0) {
		perror("socket read");
		return;
	}

	if (*command == '1') {
		/*
		 * If the summary data is ready, then return it. If not,
		 * then just return an error message.
		 */
		if (ready)
			while (scan_one(shmdata, fd, start));
		else
			write(fd, "not ready yet", 13);
	} else if (*command == '2') {
		pid_t pid = fork();

		/*
		 * Snapshot data: just kick off an instance of 'perfmon'.
		 */
		if (pid == 0) {
			if (dup2(fd, 1) < 0) {
				perror("dup2");
				exit(1);
			}
			execl("perfmon", "perfmon", 0);
			exit(1);
		} else if (pid < 0) {
			perror("fork");
			return;
		}
	} else {
		write(fd, "*error*", 7);
	}
}

int
main(int argc, char *argv[])
{
	int shmid, fd, socfd, cur = 0;
	shm_data_t shmdata[NUM_COLLECTIONS] = { 0 };
	char *temp = tempnam("/tmp", ".collector");
	int ready = 0;

	if (temp == NULL) {
		perror("could not generate temp file name");
		exit(1);
	}

	if ((fd = open(temp, O_CREAT | O_EXCL, 0666)) < 0) {
		perror("could not create temp file");
		exit(1);
	}

	socfd = open_port();

	/* Loop until killed */
	for (;;) {
		char syscmd[1024];
		union {
			struct sockaddr addr;
			char buf[512];
		} accbuf;
		int clientfd;
		socklen_t len;

		if (shmdata[cur].shmid != 0)
			shmctl(shmdata[cur].shmid, IPC_RMID, NULL);
		sprintf(syscmd, "./perfmon %s %d", temp, cur + 1);
		printf(">> %s\n", syscmd);
		system(syscmd);
		get_shm_addr(temp, cur + 1, &shmdata[cur]);
		init_lowest(&shmdata[cur]);
		cur = (cur + 1) % NUM_COLLECTIONS;
		if (cur == 0)
			ready = 1;

		clientfd = accept(socfd, &accbuf.addr, &len);
		if (clientfd >= 0) {
			serve_up(shmdata, clientfd, ready, cur);
			close(clientfd);
		}

		sleep(INTERVAL);
	}
}
