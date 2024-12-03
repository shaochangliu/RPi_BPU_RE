#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

volatile uint64_t counter = 0;
volatile int stop_counter = 0;

// Counter thread function
void* counter_thread(void* arg) {
    while (!stop_counter) {
        counter++;
    }
    return NULL;
}

// Function containing the conditional branch instruction
void perform_branch(int condition) {
    __asm__ volatile(
        "cmp %0, #0\n\t"  // Compare condition with 0
        "beq 1f\n\t"      // Branch if equal (condition == 0)
        "nop\n\t"         // Never executed if condition == 0
        "1:\n\t"          // Label 1
        :
        : "r" (condition)
        : "cc", "memory"
    );
}

void train_branch_predictor(int iterations, int condition) {
    for (int i = 0; i < iterations; i++) {
        perform_branch(condition);
    }
}

uint64_t measure_single_branch_time(int condition) {
    uint64_t start_time, end_time;

    start_time = counter;
    perform_branch(condition);
    end_time = counter;

    return end_time - start_time;
}

int main() {
    pthread_t thread;
    uint64_t time_diff;

    // Start the counter thread
    pthread_create(&thread, NULL, counter_thread, NULL);

    for (int iteration = 0; iteration < 10; iteration++)
    {
        printf("iteration: %d\n", iteration);

        // Measure time for branch with condition == 1 (no training)
        time_diff = measure_single_branch_time(1);
        printf("Branch time with no training: %lu nanoseconds\n", time_diff);

        // Measure time for branch with condition == 0 (no training)
        time_diff = measure_single_branch_time(0);
        printf("Branch time with no training: %lu nanoseconds\n", time_diff);

        // Train the branch predictor with condition == 1
        train_branch_predictor(1000, 1);

        // Measure time for branch with condition == 1 (same direction as training)
        time_diff = measure_single_branch_time(1);
        printf("Branch time with same direction as training: %lu nanoseconds\n", time_diff);

        // Measure time for branch with condition == 0 (different direction from training)
        time_diff = measure_single_branch_time(0);
        printf("Branch time with different direction from training: %lu nanoseconds\n", time_diff);
    }

    // Stop the counter thread
    stop_counter = 1;
    pthread_join(thread, NULL);

    return 0;
}