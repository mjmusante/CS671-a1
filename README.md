# CS671-a1

This is my project for BU MET CS671, Spring 2015 semester.

The first part of the project was to write a program to gather data from the /proc
filesystem. This required looping over all proesses and reading the various stat
objects within it. The output is a csv-like form containing the stats that were
gathered, along with the command line arguments.

The second part of the project was to write a program to start the stats-gathering
program periodically and redirect the output to a file. The requirement was that
we should not change the stats-gathering program: it still writes to its stdout.
We were asked to use dup2() to perform the i/o redirection.

The third part of this project was to write an "agent" program which would start
the stats-gathering program 15 times in 15 minutes. The requirement was to modify
the stats-gathering program to allow it to report its results in shared memory.
The agent program would then attach to the shared memory in order to examine
the results. After 15 minutes, the agent program then outputs a summary of all
the results over those 15 minutes.

The fourth part of this project was to use socket communication to allow the
agent program to gather stats from daemons running on machines over a network.
The agent should be able to request a snapshot of the current stats, or request
a summary of the last 15 minutes of stats.
