#include "types.h"
#include "user.h"
#include "stat.h"
#include "fcntl.h"

int main(int argc, char const *argv[])
{
    const char *source_file = argv[1];
    const char *destination_directory = argv[2];
    if (move_file(source_file, destination_directory) == 0)
    {
        printf(1, "success\n");
    }
    else
    {
        printf(1, "Moving file fails!\n");
    }
    exit();
    return 0;
}
