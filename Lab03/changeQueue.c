
#include "types.h"
#include "stat.h"
#include "user.h"


int main()
{
    change_queue(getpid(),1);
    exit();
    return 0;
}