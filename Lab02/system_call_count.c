#include "types.h"
#include "syscall.h"
#include "user.h"

void child_process(int id) {
    exit(); 
}

int main() {
    int pid;
    pid = fork();
    if (pid < 0) {
        printf(1, "Fork failed\n");
        exit();
    } else if (pid == 0) {
        child_process(pid);
    }
    sleep(5);
    list_all_processes(); // This should print the PIDs and syscall counts
    wait();
    exit(); // Exit the parent process
}


