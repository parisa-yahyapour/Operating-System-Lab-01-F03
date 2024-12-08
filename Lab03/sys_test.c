#include "types.h"
#include "syscall.h"
#include "user.h"

int main()
{
    print_process_info();
    set_process_parameter(getpid(), 100, 4);
    change_queue(getpid(),2);
    print_process_info();

    exit();
    return 0;
}