#include "types.h"
#include "stat.h"
#include "user.h"
#include "spinlock.h"
int num_recursive_call = 0;

struct reentrantlock fiblock;

int fibonacci_recursive(int n)
{
    int result;

    acquire_reentrant_lock(&fiblock);

    if (n == 0)
    {
        num_recursive_call++;
        result = 0;
    }
    else if (n == 1)
    {
        num_recursive_call++;
        result = 1;
    }
    else
    {
        num_recursive_call++;
        result = fibonacci_recursive(n - 1) + fibonacci_recursive(n - 2);
    }

    release_reentrant_lock(&fiblock);

    return result;
}

int main()
{
    int n = 2;

    init_reentrant_lock(&fiblock, "fiblock");

    printf(1, "Fibonacci series up to %d terms:\n", n);

    for (int i = 0; i <= n; i++)
    {
        printf(1, "%d ", fibonacci_recursive(i));
    }

    printf(1, "\n");
    printf(1, "recursive call: %d\n",num_recursive_call);

    exit();
}
