#include "types.h"
#include "user.h"

int main(int argc, char *argv[]) {
    int pid = atoi(argv[1]);
    sort_syscalls(pid);
    exit();
}