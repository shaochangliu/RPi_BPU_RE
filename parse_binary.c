#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <libelf.h>
#include <gelf.h>

#define TARGET_ADDRESS 0x80000000

void (*func_ptr)(int);

void load_function(const char *filename, const char *func_name) {
    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "ELF library initialization failed: %s\n", elf_errmsg(-1));
        exit(EXIT_FAILURE);
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    Elf *e = elf_begin(fd, ELF_C_READ, NULL);
    if (!e) {
        fprintf(stderr, "elf_begin() failed: %s\n", elf_errmsg(-1));
        close(fd);
        exit(EXIT_FAILURE);
    }

    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    while ((scn = elf_nextscn(e, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB) {
            Elf_Data *data = elf_getdata(scn, NULL);
            int count = shdr.sh_size / shdr.sh_entsize;
            for (int i = 0; i < count; ++i) {
                GElf_Sym sym;
                gelf_getsym(data, i, &sym);
                if (strcmp(func_name, elf_strptr(e, shdr.sh_link, sym.st_name)) == 0) {
                    void *mem = mmap((void *)TARGET_ADDRESS, sym.st_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                    if (mem == MAP_FAILED) {
                        perror("mmap");
                        elf_end(e);
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    lseek(fd, sym.st_value, SEEK_SET);
                    read(fd, mem, sym.st_size);
                    func_ptr = (void (*)(int))mem;
                    break;
                }
            }
            break;
        }
    }

    elf_end(e);
    close(fd);
}

int main() {
    load_function("branch.o");
    func_ptr(0);
    return 0;
}