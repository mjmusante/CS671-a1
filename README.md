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
