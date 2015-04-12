/*
 * Author: Mark Musante
 * Class: BU MET CS671, Spring 2015
 */

#ifndef _PROCDATA_H
#define _PROCDATA_H

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

#endif	/* _PROCDATA_H */
