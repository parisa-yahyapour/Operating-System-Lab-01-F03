#include "types.h"
#include "user.h"
#include "stat.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    int b = count_syscalls();
    printf(1, "retrun:%d\n", b);
    char *write_data = "Hi everyone. This is MMD.\n";
    int fd = open("file.txt", O_CREATE | O_WRONLY);

    for (int i = 0; i < 3; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            volatile long long int temp = 0;

            while ((open("lockfile", O_CREATE | O_WRONLY)) < 0)
            {
                temp++;
            }

            write(fd, write_data, strlen(write_data));

            unlink("lockfile");
            exit();
        }
    }

    while (wait() != -1)
        ;

    int a = count_syscalls();
    printf(1, "return:%d\n", a);
    exit(); 
}