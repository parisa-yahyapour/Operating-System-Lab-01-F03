#include "types.h"
#include "stat.h"
#include "user.h"
#define MEM_ID 0

void calculate_factorial(int index, int num)
{
    char *val = 0;

    open_sharedmem(MEM_ID, &val);

    int *shared_val = (int *)val; 
    *shared_val *= num;           
    printf(1, "Process %d updated factorial to: %d\n", index, *shared_val);

    close_sharedmem(MEM_ID);        
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf(1, "Usage: %s <number>\n", argv[0]);
        exit();
    }

    int number = atoi(argv[1]);
    if (number < 0)
    {
        printf(1, "Error: Factorial is not defined for negative numbers.\n");
        exit();
    }

    printf(1, "Starting factorial calculation for %d\n", number);

    char *val = 0;

    open_sharedmem(MEM_ID, &val);   

    int *shared_val = (int *)val;   
    *shared_val = 1;                

    for (int i = 1; i <= number; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            calculate_factorial(i, i);
            exit();
        }
    }
    for (int i = 1; i <= number; i++)
    {
        wait(); 
    }
    exit();
}