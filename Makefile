#
# Author: Mark Musante
# Class: BU MET CS671, Spring 2015
#

CC=gcc
CFLAGS=-std=c99 -g

all: perfmon monitor readshm agent

perfmon: perfmon.o

monitor: monitor.o

readshm: readshm.o

agent: agent.o
