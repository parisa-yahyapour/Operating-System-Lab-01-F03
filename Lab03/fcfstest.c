#include "types.h"
#include "stat.h"
#include "user.h"

int main() {
    int pid;

    printf(1, "Parent process starts\n");

    // Create the first child
    pid = fork();
    if (pid == 0) { // Child process
        printf(1, "Child 1 starts\n");
        for (volatile int i = 0; i < 100000000; i++); // Simulate work
        printf(1, "Child 1 ends\n");
        exit();
    }

    // Create the second child
    pid = fork();
    if (pid == 0) { // Child process
        printf(1, "Child 2 starts\n");
        for (volatile int i = 0; i < 50000000; i++); // Simulate shorter work
        printf(1, "Child 2 ends\n");
        exit();
    }

    // Parent waits for all children
    while (wait() > 0);
    printf(1, "Parent process ends\n");

    exit();
}

// #include "types.h"
// #include "stat.h"
// #include "user.h"

// #define NUM_CHILDREN 5

// int main() {
//     int pid;

//     printf(1, "Parent process starts\n");

//     // Create multiple child processes
//     for (int i = 0; i < NUM_CHILDREN; i++) {
//         pid = fork();
//         if (pid == 0) { // Child process
//             printf(1, "Child %d starts\n", i);
//             for (volatile int j = 0; j < 100000000; j++); // Simulate work
//             printf(1, "Child %d ends\n", i);
//             exit();
//         }
//     }

//     // Parent waits for all children
//     while (wait() > 0);
//     printf(1, "Parent process ends\n");

//     exit();
// }