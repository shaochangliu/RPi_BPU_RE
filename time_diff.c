#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

// Function to get current time in nanoseconds
uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
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

    start_time = get_time_ns();
    perform_branch(condition);
    end_time = get_time_ns();

    return end_time - start_time;
}

int main() {
    uint64_t time_diff;

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

    return 0;
}