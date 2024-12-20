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
#define TARGET_ADDRESS 0x10000000   // mmap needs the address to be aligned to a page boundary
#define MAX_INDEX_BITS 26
#define MAX_FUNC_PTR_NUM 20
void (*perform_branch[MAX_FUNC_PTR_NUM])();

void load_function(const char *filename, const char *func_name, int index_bits)
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
                    // printf("Symbol name: %s\n", func_name);
                    printf("Symbol size: %zu\n", sym.st_size);
                    if (sym.st_size >= (1 << index_bits))
                    {
                        fprintf(stderr, "Function size is too large for the given index bits\n");
                        elf_end(e);
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    // Find the section containing the symbol
                    Elf_Scn *sym_scn = elf_getscn(e, sym.st_shndx);
                    GElf_Shdr sym_shdr;
                    gelf_getshdr(sym_scn, &sym_shdr);

                    // Calculate the file offset of the symbol
                    off_t offset = sym_shdr.sh_offset + (sym.st_value - sym_shdr.sh_addr);
                    // printf("Symbol offset: %ld\n", offset);

                    // Calculate the memory we need to put all the functions
                    size_t size = (1 << index_bits) * MAX_FUNC_PTR_NUM;
                    size = (size + 0xfff) & ~0xfff; // Align to page size
                    void *mem = mmap((void *)TARGET_ADDRESS, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                    if (mem == MAP_FAILED)
                    {
                        perror("mmap");
                        elf_end(e);
                        close(fd);
                        exit(EXIT_FAILURE);
                    }
                    memset(mem, 0, size);

                    lseek(fd, offset, SEEK_SET);
                    ssize_t bytes_read = read(fd, mem, sym.st_size);
                    if (bytes_read != sym.st_size)
                    {
                        fprintf(stderr, "Failed to read function code: expected %zu bytes, got %zd bytes\n", sym.st_size, bytes_read);
                        elf_end(e);
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    // /*
                    printf("Read content:\n");
                    for (ssize_t j = 0; j < bytes_read; ++j)
                    {
                        printf("%02x ", ((unsigned char *)mem)[j]);
                    }
                    printf("\n");
                    // */

                    perform_branch[0] = (void (*)())mem;
                    // Clear instruction cache
                    __builtin___clear_cache(mem, (char *)mem + sym.st_size);

                    for (int j = 1; j < MAX_FUNC_PTR_NUM; j++)
                    {
                        memcpy((char *)mem + j * (1 << index_bits), mem, sym.st_size);
                        perform_branch[j] = (void (*)())((char *)mem + j * (1 << index_bits));
                        // Clear instruction cache
                        __builtin___clear_cache((char *)mem + j * (1 << index_bits), (char *)mem + j * (1 << index_bits) + sym.st_size);
                    }

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

double measure_branch_time(int iterations, int branch_num)
{
    uint64_t start_time, end_time, total_time = 0;

    for (int i = 0; i < iterations; i++)
    {
        start_time = read_cntvct();
        #pragma GCC unroll 64
        for (int j = 0; j < branch_num; j++)
            perform_branch[j]();
        end_time = read_cntvct();
        total_time += end_time - start_time;
    }

    return 1.0 * total_time / (iterations * branch_num);
}

int main()
{
    double avg_time;
    int branch_num = 2;

    // Bind the process to CPU 0
    bind_to_cpu(0);

    // Load the function containing the branch instruction
    load_function("branch.o", "perform_branch", MAX_INDEX_BITS);

    for (; branch_num <= MAX_FUNC_PTR_NUM; branch_num++)
    {
        // Measure the time taken for branches
        avg_time = measure_branch_time(TRIALS, branch_num);
        printf("Number of branches: %d, Average time for each branch: %lf\n", branch_num, avg_time);
    }

    return 0;
}