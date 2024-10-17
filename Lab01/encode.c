#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    int key_val=18;// the sum of 2-digits mod23 (56+51+93)%23=18
    unlink("result.txt");
    int fd = open("result.txt", O_CREATE | O_RDWR);
    if (fd < 0)
    {
        printf(2, "Can't open the file");
        exit();
    }
    for (int j = 1; j < argc; j++)
    {
        char *str1 = argv[j];
        int i = 0;
        while (str1[i] != '\0')
        {
            int inital_index = (int)(str1[i]);
            char a = 'a';
            char A = 'A';
            char z = 'z';
            char Z = 'Z';
            char result[2];
            int index = key_val + (int)(str1[i]);
            if (((int)a <= inital_index) && (inital_index <= (int)z))
            {
                if (index <= (int)z)
                {
                    result[0] = (char)(index);
                }
                else
                {
                    int difference = index - (int)z;
                    int new_index = (int)a + difference-1;
                    result[0] = (char)(new_index);
                }
            }
            else if (((int)A <= inital_index) && (inital_index <= (int)Z))
            {
                if (index <= (int)Z)
                {
                    result[0] = (char)(index);
                }
                else
                {
                    int difference = index - (int)Z;
                    int new_index = (int)A + difference-1;
                    result[0] = (char)(new_index);
                }
            }
            else
            {
                result[0]=str1[i];
            }
            result[1] = '\0';
            write(fd, result, 1);
            i++;
        }
        if (j == argc - 1)
        {
            char space[2];
            space[0] = '\n';
            space[1] = '\0';
            write(fd, space, 1);
        }
        else
        {
            char space[2];
            space[0] = (char)(32);
            space[1] = '\0';
            write(fd, space, 1);
        }
    }
    close(fd);
    exit();
}