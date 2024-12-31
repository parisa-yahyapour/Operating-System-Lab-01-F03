#include "types.h"
#include "stat.h"
#include "user.h"
#include "spinlock.h"
// #define NUM_FIB 3 // Number of Fibonacci numbers to calculate

// struct reentrantlock testlock; // Declare a reentrant lock
// int fib[NUM_FIB];             // Shared resource (Fibonacci series)


// // Function to calculate Fibonacci numbers
// int calculate_fibonacci(int n) {
//     acquire_reentrant_lock(&testlock);

//     if (n == 0 || n == 1) {
//         fib[n] = n;
//         return n;
//     }


//     // Acquire the lock before modifying the shared resource
//     fib[n] = calculate_fibonacci(n - 1) + calculate_fibonacci(n-2);
//     printf(1,"%d\n",fib[n]);
//     release_reentrant_lock(&testlock);

//     return fib[n];
// }

// int main() {
//     printf(1, "Initializing reentrant lock...\n");
//     init_reentrant_lock(&testlock, "fiblock");

//     // Initialize the first two Fibonacci numbers
//     fib[0] = 0;
//     fib[1] = 1;

//     printf(1, "Calculating Fibonacci series using reentrant lock...\n");

//     // Fork multiple processes to compute the Fibonacci series
//     for (int i = 2; i < NUM_FIB; i++) {
//         if (fork() == 0) {
//             calculate_fibonacci(i);
//             exit();
//         }
//     }

//     // Wait for all child processes to finish
//     for (int i = 2; i < NUM_FIB; i++) {
//         wait();
//     }

//     // Print the Fibonacci series
//     printf(1, "Fibonacci series:\n");
//     for (int i = 0; i < NUM_FIB; i++) {
//         printf(1, "%d ", fib[i]);
//     }
//     printf(1, "\n");

//     exit();
// }


struct reentrantlock fiblock; // Shared reentrant lock

int fibonacci_recursive(int n) {
    int result;

    // Acquire the reentrant lock
    acquire_reentrant_lock(&fiblock);

    // Base cases for recursion
    if (n == 0) {
        result = 0;
    } else if (n == 1) {
        result = 1;
    } else {
        // Recursive calculation with reentrant lock
        result = fibonacci_recursive(n - 1) + fibonacci_recursive(n - 2);
    }

    // Release the reentrant lock
    release_reentrant_lock(&fiblock);

    return result;
}

int main() {
    int n = 10; // Number of Fibonacci terms to calculate

    // Initialize the reentrant lock
    init_reentrant_lock(&fiblock, "fiblock");

    printf(1, "Fibonacci series up to %d terms:\n", n);

    // Calculate and print Fibonacci terms
    for (int i = 0; i <= n; i++) {
        printf(1, "%d ", fibonacci_recursive(i));
    }

    printf(1, "\n");

    exit();
}

