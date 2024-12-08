
#include "types.h"
#include "stat.h"
#include "user.h"

void test_process(int id) {
    long long count = 0;
    while (count!=10000000000) {
      printf(1, "1");
        count++;
    }
       count = 0;
    // while (count!=1000000000000000000) {
    //   printf(1, "");
    //     count++;
    // }    count = 0;
    // while (count!=1000000000000000000) {
    //   printf(1, "");
    //     count++;
    // }    count = 0;
    // while (count!=1000000000000000000) {
    //   printf(1, "");
    //     count++;
    // }   count = 0;
    // while (count!=1000000000000000000) {
    //   printf(1, "");
    //     count++;
    // }
}

int main(void) {
    int i;
    // Create 3 test processes
    for (i = 0; i < 3; i++) {
        if (fork() == 0) {
            test_process(i);
            exit();
        }
    }

    // Parent process does nothing, just waits for children to terminate
    for (i = 0; i < 3; i++) {
        wait();
    }

    exit();
}

