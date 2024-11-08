#include "user.h"
#include "types.h"

int main(int argc, char* argv[])
{
    int pid = getpid();
    printf(1, "Process ID: %d,\n", pid);
    exit();
}