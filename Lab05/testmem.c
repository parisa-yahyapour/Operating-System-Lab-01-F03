#include "types.h"
#include "stat.h"
#include "user.h"
#define MEM_ID 0

void calculate_factorial(int index, int num)
{
    char *val = 0;

    open_sharedmem(MEM_ID, &val);   // Open shared memory
    acquire_sharedmem_lock(MEM_ID); // Lock shared memory

    int *shared_val = (int *)val; // Interpret the shared memory as an integer pointer
    *shared_val *= num;           // Multiply the current value in shared memory by `num`
    printf(1, "Process %d updated factorial to: %d\n", index, *shared_val);

    release_sharedmem_lock(MEM_ID); // Release the lock
    close_sharedmem(MEM_ID);        // Close shared memory
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

    open_sharedmem(MEM_ID, &val);   // Open shared memory
    acquire_sharedmem_lock(MEM_ID); // Lock shared memory

    int *shared_val = (int *)val;   // Interpret the shared memory as an integer pointer
    *shared_val = 1;                // Initialize shared memory with 1 (factorial base case)

    printf(1, "ghaa\n");
    release_sharedmem_lock(MEM_ID); // Release the lock
    printf(1, "sss\n");
    for (int i = 1; i <= number; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            // Child process performs its part of the factorial computation
            calculate_factorial(i, i);
            sleep(5);
            exit();
        }
    }

    for (int i = 1; i <= number; i++)
    {
        wait(); // Wait for all child processes to complete
    }
    // sleep(5);
    // acquire_sharedmem_lock(MEM_ID); // Lock shared memory
    // printf(1, "Final factorial result for %d is: %d\n", number, *shared_val);

    // release_sharedmem_lock(MEM_ID); // Release the lock
    // close_sharedmem(MEM_ID); // Close shared memory

    exit();
}