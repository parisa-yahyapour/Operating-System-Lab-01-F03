#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define NUM_PROCESSES 5

void test_sjf() {
    int pids[NUM_PROCESSES];
    //int time_bursts[NUM_PROCESSES] = {10, 5, 15, 8, 12};  // Example burst times
    //int confidences[NUM_PROCESSES] = {5, 5, 50, 70, 60}; // Example confidences (percentage)

    printf(1, "SJF Test: Creating processes...\n");

    for (int i = 0; i < NUM_PROCESSES; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            // Child process
            //printf(1, "Process %d started with time_burst=%d and confidence=%d%%\n", getpid(), time_bursts[i], confidences[i]);
            printf(1, "Process %d started\n", getpid());
            // Set time_burst and confidence using the system call
            //set_process_parameter(getpid(), time_bursts[i], confidences[i]);

            // Simulate process work
            for (int j = 0; j < 10000000; j++) 
            {
                printf(1, "");
                
            } // Busy loop
            printf(1, "Process %d finished\n", getpid());
            exit();
        }

    }

    // Parent process
    for (int i = 0; i < NUM_PROCESSES; i++) {
        wait(); // Wait for all child processes to finish
    }

    printf(1, "SJF Test completed.\n");
}

int main() {
    test_sjf();
    exit();
}