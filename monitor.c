#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>

#define	PERFMON "perfmon"
#define	DEFAULT_INTERVAL 30
#define	DEFAULT_DURATION 86400
#define	DEFAULT_OUTPUT_TEMPLATE "%T_proc.dat"
#define	MAX_LENGTH 1024
#define	TIMESTAMP_FORMAT "%a, %d %b %y %T %z"

/*
 * Function:	gather_data
 * Description:	Use the PERFMON program to write data to a file
 * Input:	file descriptor of output
 * Returns:	void
 */
void
gather_data(int fd)
{
	pid_t pid = fork();
	int status;
	time_t now;
	char timestamp[MAX_LENGTH];

	if (pid == 0) {
		/* redirect stdout to the file fd */
		if (dup2(fd, 1) < 0) {
			perror("unable to dup2()");
			exit(1);
		}
		now = time(0);
		strftime(timestamp, MAX_LENGTH, TIMESTAMP_FORMAT,
		    localtime(&now));
		printf("%s\n", timestamp);
		execl(PERFMON, PERFMON, 0);
		exit(1);
	} else if (pid < 0) {
		perror("unable to fork()");
		exit(1);
	}

	(void) wait(&status);
}

/*
 * Usage:
 * 	monitor [interval [duration [output]]]
 *
 * Command line arguments:
 * 	- interval: time, in seconds, between gathering of statistics
 * 			(default is 30)
 * 	- duration: time, in seconds, that the program should
 * 			(default is 86400)
 * 	- output: name of the output file to write to
 * 			(default is <start-time>_proc.dat)
 */
int
main(int argc, char *argv[])
{
	time_t start_time;
	int interval = DEFAULT_INTERVAL;
	int duration = DEFAULT_DURATION;
	int fd;
	char pathname[MAX_LENGTH], *path;

	start_time = time(0);

	if (argc > 1) {
		/* first argument is the interval */
		interval = atoi(argv[1]);
		if (interval < 1) {
			fprintf(stderr, "Invalid interval (must be > 0)\n");
			exit(1);
		}
	}

	if (argc > 2) {
		/* second argument is the duration */
		duration = atoi(argv[2]);
		if (duration < interval) {
			fprintf(stderr, "Invalid duration (must be > %d)\n",
			    interval);
			exit(1);
		}
	}

	if (argc > 3) {
		path = argv[3];
	} else {
		/* third argument is the output filename */
		strftime(pathname, MAX_LENGTH, DEFAULT_OUTPUT_TEMPLATE,
		    localtime(&start_time));
		path = pathname;
	}

	if (argc > 4) {
		fprintf(stderr, "Too many arguments\n");
		exit(1);
	}

	/*
	 * Open the output file.
	 */
	fd = open(path, O_WRONLY | O_TRUNC | O_APPEND | O_CREAT | O_EXCL);
	if (fd < 0) {
		perror("Cannot create output file");
		exit(1);
	}

	printf("Writing output to: %s\n", path);

	while (time(0) < start_time + duration) {

		/* gather data, measuring how long it takes */
		int start = time(0);
		gather_data(fd);
		int end = time(0);

		/* remaining time is (interval - (end - start)) */
		sleep(interval - (end - start));
	}
	close(fd);

	exit(0);
}
