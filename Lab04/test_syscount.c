// #include "types.h"
// #include "user.h"

// int main(int argc, char const *argv[])
// {
//     int a= count_syscalls();
//     printf(1,"%d",a);
//     exit();
//     return 0;
// }

#include "types.h"
#include "user.h"
#include "stat.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    int b = count_syscalls();
    printf(1, "%d\n", b);
    char *write_data = "Hi everyone. This is MMD.\n";
    int fd = open("file.txt", O_CREATE | O_WRONLY); // Open or create 'file.txt' in write-only mode.

    for (int i = 0; i < 3; i++)
    {
        int pid = fork(); // Fork a new process.
        if (pid == 0)
        { // Child process
            volatile long long int temp = 0;

            // Busy-wait loop until the lockfile is available
            while ((open("lockfile", O_CREATE | O_WRONLY)) < 0)
            {
                temp++; // Incrementing 'temp' for activity during busy-waiting
            }
  
            // Critical section: writing to file.txt
            write(fd, write_data, strlen(write_data)); // Write to the file.

            // Release the lock by deleting the lockfile
            unlink("lockfile");
            exit(); // Exit the child process.
        }
    }

    // Parent process waits for all child processes to complete
    while (wait() != -1)
        ;

    close(fd); // Close the file descriptor for 'file.txt'.
    int a = count_syscalls();
    printf(1, "%d\n", a);
    exit(); // Exit the parent process.
}