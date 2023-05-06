## UID: XXXXXXXXX

## You Spin Me Round Robin

This program simulates the round robin scheduling algorithm. It parses input containing PIDs, arrival times, and burst times taken from a text file. It prints the average waiting time, which is the average of how long each process spent in the queue, and the average response time, which is the average of how long each process took before it was first run.

## Building

To build the executable, simply run `make` in the folder where the Makefile and all other files are present.

## Running

To run the program, run `./rr <processes input file> <quantum length>.`

For example:

<img width="327" alt="image" src="https://user-images.githubusercontent.com/34567765/236612771-344803f7-5d45-4522-ad42-f42a1fa8dd48.png">

## Cleaning up

To clean up all binary files, run `make clean`.
