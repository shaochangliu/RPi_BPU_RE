#define _GNU_SOURCE

#include <err.h>
#include <sched.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define TRIALS 10000

// Function to bind the process to a specific CPU
void bind_to_cpu(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) < 0)
        err(EXIT_FAILURE, "Unable to set CPU affinity");
}

// Function to get current time; similar to rdtscp
__attribute__((always_inline)) inline uint64_t read_cntvct(void) {
    uint64_t val;
    asm volatile("dsb ish" ::: "memory");
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));    // Barrier before and after reading the counter
    asm volatile("dsb ish" ::: "memory");
    return val;
}

//////////////////////////////////////////////
// xorshift128+ by Sebastiano Vigna
// from http://xorshift.di.unimi.it/xorshift128plus.c
uint64_t s[2] = {0, 1};

__attribute__((always_inline)) inline uint64_t xrand(void) {
    uint64_t s1 = s[0];
    const uint64_t s0 = s[1];
    s[0] = s0;
    s1 ^= s1 << 23; // a
    s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5); // b, c
    return s[1] + s0;
}

// splitmix64 generator -- http://xorshift.di.unimi.it/splitmix64.c
void xsrand(uint64_t x) {
    for (int i = 0; i <= 1; i++) {
        uint64_t z = (x += UINT64_C(0x9E3779B97F4A7C15));
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        s[i] = z ^ (z >> 31);
    }
}
/////////////////////////////////////

// Function containing the conditional branch instruction
/*
void perform_branch(int condition)
{
    __asm__ volatile(
        "cmp %0, #0\n\t" // Compare condition with 0
        "beq 1f\n\t"     // Branch if equal (condition == 0)
        "nop\n\t"        // Never executed if condition == 0
        "add sp, sp, #0x10\n\t"
        "ret\n\t"       // Make sure both directions have the same number of instructions
        "1:\n\t"        // Label 1
        :
        : "r"(condition)
        : "cc", "memory");
}
*/

// Function containing the conditional branch instruction; implemented in .s file
void perform_branch(int condition);

void train_branch_predictor(int iterations, int condition)
{
    for (int i = 0; i < iterations; i++)
    {
        perform_branch(condition);
    }
}

uint64_t measure_single_branch_time(int condition)
{
    uint64_t start_time, end_time;

    start_time = read_cntvct();
    perform_branch(condition);
    end_time = read_cntvct();

    return end_time - start_time;
}

int main()
{
    uint64_t time_diff;
    int rand;
    int results[TRIALS][2];
    int total_branch_time[2];

    // Bind the process to CPU 0
    bind_to_cpu(0);

    // Random # generator
    xsrand(time(NULL));

    for (int iteration = 0; iteration < TRIALS; iteration++)
    {
        rand = (int) xrand() % 2;

        // Train the branch predictor with condition == 1
        train_branch_predictor(100, 1);

        // Measure time for branch with condition == 1 (same direction as training)
        time_diff = measure_single_branch_time(1);
        results[iteration][0] = (int) time_diff;

        // Measure time for unpredictable branch
        time_diff = measure_single_branch_time(rand);
        results[iteration][1] = (int) time_diff;
    }

    for (int i = 0; i < TRIALS; i++)
    {
        total_branch_time[0] += results[i][0];
        total_branch_time[1] += results[i][1];
    }

    double avg_time_unpredictable = (double) total_branch_time[1] / TRIALS;
    int unpredictable_branches_below_avg = 0;

    for (int i = 0; i < TRIALS; i++)
        if (results[i][1] < avg_time_unpredictable)
            unpredictable_branches_below_avg++;

    printf("Average time for predictable branch: %d\n", total_branch_time[0] / TRIALS);
    printf("Average time for unpredictable branch: %f\n", avg_time_unpredictable);
    printf("Correct prediction rate for unpredictable branch: %f%%\n", (double) unpredictable_branches_below_avg / TRIALS);

    return 0;
}