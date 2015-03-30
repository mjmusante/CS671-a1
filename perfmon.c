/*
 * Author: Mark Musante
 * Class: BU MET CS671, Spring 2015
 */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>

/*
 * Global: number of clock ticks per second
 */
static long Clock_Tick;

/*
 * Global: size in bytes for a memory page
 */
static long Page_Size;

/*
 * Struct: per-process data
 */
typedef struct procdata_s {
	pid_t pid;			// process pid
	pid_t ppid;			// parent process pid
	pid_t pgrp;			// process group
	unsigned long long starttime;	// start time (tv_sec)
	double utime;			// user-time in seconds
	double stime;			// system-time in seconds
	unsigned long long rss;		// rss size in bytes
	unsigned long long text;	// text size in bytes
	unsigned long long data;	// data size in bytes
	unsigned long long shared;	// shared size in bytes
	unsigned long long library;	// library size in bytes
	unsigned long majflt;		// major page faults
	unsigned long minflt;		// minor page faults
	uid_t ruid;			// real user id
	uid_t euid;			// effective user id
	gid_t rgid;			// real group id
	gid_t egid;			// effective group id
	char *command;			// full command
} procdata_t;

/*
 * Forward declarations for reading each type of data
 */
static void get_stat_info(FILE *fp, procdata_t *data);
static void get_statm_info(FILE *fp, procdata_t *data);
static void get_status_info(FILE *fp, procdata_t *data);
static void get_command_line(FILE *fp, procdata_t *data);

/*
 * Each file in /proc/[pid]/... has its own format, so I divided up
 * the functions so each one reads a different file and extracts the
 * required data.
 */
struct procreader_s {
	char *source;
	void (*get)(FILE *fp, procdata_t *data);
} reader[] = {
	{ "stat", get_stat_info },
	{ "statm", get_statm_info },
	{ "status", get_status_info },
	{ "cmdline", get_command_line },
};
#define	NUM_READERS ((sizeof (reader)) / (sizeof (reader[0])))

/*
 * The full data for all processes are stored here in procinfo_t
 */
typedef struct procinfo_s {
	int count;
	int alloc;
	procdata_t *pd;
} procinfo_t;

/*
 * Funciton:	copy_string
 * Description:	makes an identical copy of a string
 * Input:	str = pointer to a string
 * Returns:	pointer to malloc'd region containing a copy
 */
static char *
copy_string(const char *str)
{
	int len = strlen(str);
	char *result = malloc(len + 1);
	strcpy(result, str);
	return (result);
}

/*
 * Function:	get_command_line
 * Description:	Retrieve full command line data
 * Input:	fp = open file, data = results structure
 * Returns:	void
 */
static void
get_command_line(FILE *fp, procdata_t *data)
{
	char *command;
	int hit_null = 0;
	int count = 0, cursize = MAXPATHLEN;

	/* not sure how much room we need, so start with MAXPATHLEN */
	command = malloc(cursize);

	/* loop over the file, getting one character at a time */
	char ch, *p = command;
	while ((ch = fgetc(fp)) != EOF) {

		/* if we reached the end of the command, break */
		if (ch == '\0' && hit_null)
			break;

		/* if we hit a null byte, turn on the flag and continue */
		if (ch == '\0') {
			hit_null = 1;
			continue;
		}

		/* if the previous char was null, add a space to the command */
		if (hit_null) {
			*(p++) = ' ';
			++count;
			hit_null = 0;
		}

		/* save this character in the command lind */
		*(p++) = ch;
		++count;

		/* if we need more space, realloc a bigger buffer */
		if (count + 3 >= cursize) {
			cursize += MAXPATHLEN;
			command = realloc(data->command, cursize);
		}
	}

	/* don't forget to null-terminate the string */
	*p = '\0';

	/*
	 * If we found a command, make a copy of the string (this saves
	 * space because we malloc'd more than we actually needed). If
	 * we didn't find a command, then we keep what we originally found
	 * in /proc/[pid]/stat (see get_stat_info function).
	 */
	if (strlen(command) > 1) {
		free(data->command);
		data->command = copy_string(command);
	}

	/* free up the scratchpad area we were using */
	free(command);
}

/*
 * Function:	get_status_info
 * Description:	Get UID and GID from /proc/[pid]/status
 * Input:	fp = open file, data = results structure
 * Returns:	void
 */
static void
get_status_info(FILE *fp, procdata_t *data)
{
	char line[MAXPATHLEN];
	int got_uid = 0;
	int got_gid = 0;

	/* read a line at a time */
	while (fgets(line, MAXPATHLEN, fp) != NULL) {

		/* look for the "Uid:" or "Gid:" strings */
		if (strncmp(line, "Uid:", 4) == 0) {
			sscanf(&line[4], "%d %d", &data->ruid, &data->euid);
			got_uid = 1;
		} else if (strncmp(line, "Gid:", 4) == 0) {
			sscanf(&line[4], "%d %d", &data->rgid, &data->egid);
			got_gid = 1;
		}

		/* once we found both strings, exit */
		if (got_uid && got_gid)
			break;
	}
}

/*
 * Function:	get_statm_info
 * Description:	Get size information
 * Input:	fp = open file, data = results structure
 * Returns:	void
 */
static void
get_statm_info(FILE *fp, procdata_t *data)
{
	unsigned long long dummyllu, textp, datap, shared, library;

	/* 
	 * The format for this file is specified in proc(5). I'm using
	 * fscanf(3) to extract the information for all the fields, and
	 * then saving the ones we need.
	 */
	fscanf(fp, "%llu %llu %llu %llu %llu %llu %llu",
	    &dummyllu,	/* size */
	    &dummyllu,	/* resident */
	    &shared,	/* share */
	    &textp,	/* text */
	    &library,	/* lib */
	    &datap,	/* data */
	    &dummyllu	/* dt */
	);

	/* don't forget that all values are in pages, so mult. by Page_Size */
	data->text = textp * Page_Size;
	data->data = datap * Page_Size;
	data->shared = shared * Page_Size;
	data->library = library * Page_Size;
}

/*
 * Function:	get_stat_info
 * Description:	Get process stats
 * Input:	fp = open file, data = results structure
 * Returns:	void
 */
static void
get_stat_info(FILE *fp, procdata_t *data)
{
	pid_t pid;
	char comm[MAXPATHLEN];
	char state;
	int dummyint;
	unsigned long dummylu, utime, stime;
	long dummyld, rss;

	/* 
	 * The format for this file is specified in proc(5). I'm using
	 * fscanf(3) to extract the information for all the fields, and
	 * then saving the ones that are needed. The 'dummy*' variables
	 * are thrown away.
	 */
	fscanf(fp, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu "
	    "%lu %lu %ld %ld %ld %ld %ld %ld %llu %lu %ld",
	    &pid, comm, &state, 
	    &data->ppid, &data->pgrp,
	    &dummyint,	/* session */
	    &dummyint,	/* tty_nr */
	    &dummyint,	/* tpgid */
	    &dummyint,	/* flags */
	    &data->minflt,
	    &dummylu,	/* cminflt */
	    &data->majflt,
	    &dummylu,	/* cmajflt */
	    &utime,	/* utime */
	    &stime,	/* stime */
	    &dummyld,	/* cutime */
	    &dummyld,	/* cstime */
	    &dummyld,	/* priority */
	    &dummyld,	/* nice */
	    &dummyld,	/* num_threads */
	    &dummyld,	/* itrealvalue */
	    &data->starttime,
	    &dummylu,	/* vsize */
	    &rss	/* rss */
	    );

	/* sanity-check that we're getting the right data */
	if (pid != data->pid) {
		data->pid = 0;	// flag this entry as invalid
		return;
	}

	/* these four values need to be adjusted to meet the requirements */
	data->command = copy_string(comm);
	data->utime = ((double)utime / (double)Clock_Tick);
	data->stime = ((double)stime / (double)Clock_Tick);
	data->rss = rss * Page_Size;
}

/*
 * Function:	get_procs
 * Description:	Retrieve all process information in the system
 * Input:	void
 * Returns:	procinfo_t structure will all the results
 */
static procinfo_t *
get_procs(void)
{
	DIR *dir = opendir("/proc");
	struct dirent *de;
	procinfo_t *pi = malloc(sizeof (procinfo_t));

	/*
	 * We don't know how many processes are on the system yet,
	 * so start off with 10. In the loop, we'll realloc more space
	 * if we run out of entries
	 */
	pi->count = 0;
	pi->alloc = 10;
	pi->pd = malloc(pi->alloc * sizeof (procdata_t));

	if (dir == NULL) {
		perror("Could not open /proc");
		exit(1);
	}

	/* Loop over each entry in the /proc directory */
	while ((de = readdir(dir)) != NULL) {

		/* attempt to convert to a number */
		int pid = atoi(de->d_name);

		/* if it's not a number, then it's not a process */
		if (pid == 0)
			continue;

		procdata_t *pd = &pi->pd[pi->count++];

		/* if we've run out of room, expand the data area */
		if (pi->count >= pi->alloc) {
			pi->alloc *= 2;
			pi->pd = realloc((void *)pi->pd,
			    pi->alloc * sizeof (procdata_t));
		}

		/* save the pid */
		pd->pid = pid;

		/* loop over each reader function */
		for (int i = 0; i < NUM_READERS; i++) {
			char filepath[MAXPATHLEN];
			sprintf(filepath, "/proc/%d/%s", pid,
			    reader[i].source);

			/* open the /proc/pid/[source] file */
			FILE *fp = fopen(filepath, "r");

			/* if it couldn't be opened, then skip this */
			if (fp == NULL) {
				pd->pid = 0;	// mark the entry as invalid
				break;
			}

			/* read the data and close the file */
			reader[i].get(fp, pd);
			fclose(fp);
		}
	}

	/* we're done - close the directory */
	closedir(dir);

	/* return a pointer to the data we collected */
	return (pi);
}

/*
 * Usage:
 * 	perfmon
 * 	
 * Command line arguments:
 * 	- none -
 */
int
main(int argc, char *argv[])
{
	/* set the globals */
	Clock_Tick = sysconf(_SC_CLK_TCK);
	Page_Size = sysconf(_SC_PAGE_SIZE);

	/* get all process information */
	procinfo_t *proc = get_procs();

	/* print it out in a comma-separated list */
	for (int i = 0; i < proc->count; i++) {
		procdata_t *pd = &proc->pd[i];
		if (pd->pid == 0)	// if entry is invalid, skip it
			continue;
		printf("%d, ", pd->pid);
		printf("%d, ", pd->ppid);
		printf("%d, ", pd->pgrp);
		printf("%llu, ", pd->starttime);
		printf("%.2f, ", pd->utime);
		printf("%.2f, ", pd->stime);
		printf("%llu, ", pd->rss);
		printf("%llu, ", pd->text);
		printf("%llu, ", pd->data);
		printf("%llu, ", pd->shared);
		printf("%llu, ", pd->library);
		printf("%lu, ", pd->majflt);
		printf("%lu, ", pd->minflt);
		printf("%d, ", pd->ruid);
		printf("%d, ", pd->euid);
		printf("%d, ", pd->rgid);
		printf("%d, ", pd->egid);
		printf("%s\n", pd->command);
		free(pd->command);	// free each command as we come to it
	}

	/* free our allocations */
	free(proc->pd);
	free(proc);

	/* success */
	return (0);
}
