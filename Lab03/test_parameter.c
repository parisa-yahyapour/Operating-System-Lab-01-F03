#include "types.h"
#include "user.h"

int main(int argc, char const *argv[])
{
    int pid, confidence, time_burst;
    pid = atoi(argv[1]);
    time_burst = atoi(argv[2]);
    confidence = atoi(argv[3]);
    set_process_parameter(pid, confidence, time_burst);
    exit();
    return 0;
}
