#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char const *argv[])
{
    int pid = atoi(argv[1]);
    int result = get_most_invoked_syscall(pid);
    if (result == -1)
    {
        printf(1, "The request failed!\n");
    }
    else
    {
        printf(1, "The most invoked system call in process with PID: %d\nThe system call code: %d\n", pid, result);
    }
    exit();
    return 0;
}
