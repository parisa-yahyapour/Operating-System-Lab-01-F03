#include "types.h"
#include "user.h"
#include "stat.h"
#include "fcntl.h"

int main(int argc, char const *argv[])
{
    const char *source_file = argv[1];
    const char *destination_directory = argv[2];
    // int new_file = open("source.txt", O_CREATE | O_WRONLY);
    // if (new_file < 0)
    // {
    //     printf(1, "Error in creating file!\n");
    //     exit();
    // }
    // write(new_file, "Hello World!\n", 14);
    // close(new_file);
    // if (mkdir("dist"))
    // {
    //     printf(1, "Error in creating dir\n");
    //     exit();
    // }
    // int new_file2 = open("dist/source.txt", O_CREATE | O_WRONLY);
    // if (new_file2 < 0)
    // {
    //     printf(1, "Error in creating destination file\n");
    // }
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
