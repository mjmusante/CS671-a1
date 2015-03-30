/*
 * Author: Mark Musante
 * Class: BU MET CS671, Spring 2015
 */

CC=gcc
CFLAGS=-std=c99

all: perfmon monitor

perfmon: perfmon.o

monitor: monitor.o
