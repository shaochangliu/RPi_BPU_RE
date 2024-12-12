#define _GNU_SOURCE

#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <sched.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define TRIALS 10000

#define TARGET_ADDRESS 0x80000000   // mmap needs the address to be aligned to a page boundary
#define BEQ_OFFSET 0x10  // Offset of the branch instruction in the binary
#define TARGET_BRANCH_ADDRESS (TARGET_ADDRESS + BEQ_OFFSET) // Address of the branch instruction

void (*perform_branch)(int);

void load_function(const char *filename, const char *func_name)
{
    if (elf_version(EV_CURRENT) == EV_NONE)
    {
        fprintf(stderr, "ELF library initialization failed: %s\n", elf_errmsg(-1));
        exit(EXIT_FAILURE);
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    Elf *e = elf_begin(fd, ELF_C_READ, NULL);
    if (!e)
    {
        fprintf(stderr, "elf_begin() failed: %s\n", elf_errmsg(-1));
        close(fd);
        exit(EXIT_FAILURE);
    }

    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    while ((scn = elf_nextscn(e, scn)) != NULL)
    {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB)
        {
            Elf_Data *data = elf_getdata(scn, NULL);
            int count = shdr.sh_size / shdr.sh_entsize;
            for (int i = 0; i < count; ++i)
            {
                GElf_Sym sym;
                gelf_getsym(data, i, &sym);
                if (strcmp(func_name, elf_strptr(e, shdr.sh_link, sym.st_name)) == 0)
                {
                    printf("Symbol name: %s\n", func_name);
                    printf("Symbol size: %zu\n", sym.st_size);

                    // Find the section containing the symbol
                    Elf_Scn *sym_scn = elf_getscn(e, sym.st_shndx);
                    GElf_Shdr sym_shdr;
                    gelf_getshdr(sym_scn, &sym_shdr);

                    // Calculate the file offset of the symbol
                    off_t offset = sym_shdr.sh_offset + (sym.st_value - sym_shdr.sh_addr);
                    printf("Symbol offset: %ld\n", offset);

                    // Ensure memory alignment
                    size_t aligned_size = (sym.st_size + 3) & ~3;
                    void *mem = mmap((void *)TARGET_ADDRESS, aligned_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                    if (mem == MAP_FAILED)
                    {
                        perror("mmap");
                        elf_end(e);
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    lseek(fd, offset, SEEK_SET);
                    ssize_t bytes_read = read(fd, mem, sym.st_size);
                    if (bytes_read != sym.st_size)
                    {
                        fprintf(stderr, "Failed to read function code: expected %zu bytes, got %zd bytes\n", sym.st_size, bytes_read);
                        elf_end(e);
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    printf("Read content:\n");
                    for (ssize_t j = 0; j < bytes_read; ++j)
                    {
                        printf("%02x ", ((unsigned char *)mem)[j]);
                    }
                    printf("\n");

                    // Clear instruction cache
                    __builtin___clear_cache(mem, (char *)mem + sym.st_size);

                    perform_branch = (void (*)(int))mem;
                    break;
                }
            }
            break;
        }
    }

    elf_end(e);
    close(fd);
}

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
__attribute__((always_inline)) inline uint64_t read_cntvct(void)
{
    uint64_t val;
    asm volatile("dsb ish" ::: "memory");
    asm volatile("mrs %0, cntvct_el0" : "=r"(val)); // Barrier before and after reading the counter
    asm volatile("dsb ish" ::: "memory");
    return val;
}

//////////////////////////////////////////////
// xorshift128+ by Sebastiano Vigna
// from http://xorshift.di.unimi.it/xorshift128plus.c
uint64_t s[2] = {0, 1};

__attribute__((always_inline)) inline uint64_t xrand(void)
{
    uint64_t s1 = s[0];
    const uint64_t s0 = s[1];
    s[0] = s0;
    s1 ^= s1 << 23;                          // a
    s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5); // b, c
    return s[1] + s0;
}

// splitmix64 generator -- http://xorshift.di.unimi.it/splitmix64.c
void xsrand(uint64_t x)
{
    for (int i = 0; i <= 1; i++)
    {
        uint64_t z = (x += UINT64_C(0x9E3779B97F4A7C15));
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        s[i] = z ^ (z >> 31);
    }
}
/////////////////////////////////////

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

    // Load the function containing the branch instruction
    load_function("branch.o", "perform_branch");
    if (perform_branch)
        printf("perform_branch is loaded to %p\n", perform_branch);
    else
    {
        fprintf(stderr, "Failed to load function\n");
        return EXIT_FAILURE;
    }

    // Bind the process to CPU 0
    bind_to_cpu(0);

    // Random # generator
    xsrand(time(NULL));

    for (int iteration = 0; iteration < TRIALS; iteration++)
    {
        rand = (int)xrand() % 2;

        // Train the branch predictor with condition == 1
        train_branch_predictor(100, 1);

        // Measure time for branch with condition == 1 (same direction as training)
        time_diff = measure_single_branch_time(1);
        results[iteration][0] = (int)time_diff;

        // Measure time for unpredictable branch
        time_diff = measure_single_branch_time(rand);
        results[iteration][1] = (int)time_diff;
    }

    for (int i = 0; i < TRIALS; i++)
    {
        total_branch_time[0] += results[i][0];
        total_branch_time[1] += results[i][1];
    }

    double avg_time_unpredictable = (double)total_branch_time[1] / TRIALS;
    int unpredictable_branches_below_avg = 0;

    for (int i = 0; i < TRIALS; i++)
        if (results[i][1] < avg_time_unpredictable)
            unpredictable_branches_below_avg++;

    printf("Average time for predictable branch: %f\n", (double)total_branch_time[0] / TRIALS);
    printf("Average time for unpredictable branch: %f\n", avg_time_unpredictable);
    printf("Correct prediction rate for unpredictable branch: %f%%\n", (double)unpredictable_branches_below_avg / TRIALS);

    return 0;
}