#include "types.h"
#include "syscall.h"
#include "user.h"

void test_process(int number)
{
    long long count = 0;
    if (number != 3)
    {
        while (count != 10000000)
        {
            printf(1, "");
            count++;
        }
    }
    else if (number == 3)
    {
        set_process_parameter(getpid(), 100, 4);
        change_queue(getpid(), 2);
        print_process_info();
    }
}

int main()
{
    print_process_info();
    int i;
    for (i = 0; i < 4; i++)
    {
        if (fork() == 0)
        {
            test_process(i);
            exit();
        }
    }

    for (i = 0; i < 4; i++)
    {
        wait();
    }
    exit();
    return 0;
}