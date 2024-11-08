#include "types.h"
#include "syscall.h"
#include "user.h"
int main(int argc, char *argv[])
{
    int number = atoi(argv[1]);
    int last_value;
    asm volatile(
        "movl %%ebx, %0;"
        "movl %1, %%ebx;"
        : "=r"(last_value)
        : "r"(number));
    create_palindrome();
    // printf(1, "The num%d\n", number);
    asm("movl %0, %%ebx"
        :
        : "r"(last_value));
    exit();
}