#include "types.h"
#include "syscall.h"
#include "user.h"

void child_process(int id) {
    for (int i = 0; i < 5; i++) {
        sleep(10); // This will count as a syscall.
    }
    exit(); 
}

int main() {
    int pid;
    for (int i = 0; i < 3; i++) {
        pid = fork();
        if (pid < 0) {
            printf(1, "Fork failed\n");
            exit();
        } else if (pid == 0) {
            child_process(i);
        }
    }

    list_all_pro(); // This should print the PIDs and syscall counts

    for (int i = 0; i < 3; i++) {
        wait(); // Wait for child processes to finish
    }
    exit(); // Exit the parent process
}


