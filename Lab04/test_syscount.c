#include "types.h"
#include "user.h"

int main(int argc, char const *argv[])
{
    int a= count_syscalls();
    printf(1,"%d",a);
    exit();
    return 0;
}