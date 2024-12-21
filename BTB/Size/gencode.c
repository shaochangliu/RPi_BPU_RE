#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{ 
    unsigned long iterations;       /* number of iterations in the loop */
    int dist;                       /* max distance in bytes between two subsequent branch instruction; >=16 */                                        
    int branches;                   /* number of branches inside a loop */
    
    char file_name[30];             /* name of the generated C file */
    FILE * fout;                    /* generated C file */
    int m;                          /* number of iterations in millions, part of the file name */

    int j, k;                       /* loop indexes */
    int num_inst, num_nop;          /* number of total instructions and nop instructions in the distance code */

    if (argc != 4) {
        printf("Usage: %s number_of_iterations distance number_of_branches\n", argv[0]);
        exit(1);
    }

    iterations = atoi(argv[1]);
    dist = atoi(argv[2]);
    branches = atoi(argv[3]);

    /* generate file name */
    m = iterations / 1000000;
    if (iterations % 1000000 != 0) {
        fprintf(stderr, "Number of iterations must be millions\n");
        exit(1);
    }

    sprintf(file_name, "T1I%dmN%dB%d.c", m, dist, branches);
    
    /* open output file */
    fout = fopen(file_name, "w");
    if (fout == NULL) {
        perror("fopen");
        exit(1);
    }
    
    /* write the content of the program */
    fprintf(fout, "#include <stdio.h>\n");
    fprintf(fout, "#include <time.h>\n");

    fprintf(fout, "int main(void) { \n");

    fprintf(fout, "clock_t start, end;\n");
    fprintf(fout, "start = clock();\n");

    fprintf(fout, "unsigned long i;\n");
    fprintf(fout, "unsigned long liter = %lu;\n", iterations);
    fprintf(fout, "for (i = 0; i < liter; ++i) {\n");
    fprintf(fout, "__asm__ volatile (\n");

    /* generate sequence of asm instructions */
    /* assume short in-loop branches, distance < 128 */
    
    num_inst = dist / 4;  // ARM instruction is 4 bytes
    num_nop = num_inst - 3;  // exclude cmp, ble and label instructions

    fprintf(fout, "\"mov w0, #10\\n\\t\"\n");
    fprintf(fout, "\"cmp w0, #15\\n\\t\"\n");
    
    for (j = 0; j < branches - 1; j++) {
        fprintf(fout, "\"ble l%d\\n\\t\"\n", j);

        /* generate noops */
        /* short branch is four bytes */
        
        for (k = 1; k <= num_nop; k++) { 
            fprintf(fout, "\"nop\\n\\t\"\n");
        }
        
        /* we can always generate a label (even for the last iteration - no harm) */
        fprintf(fout, "\"l%d: \\n\\t\"\n", j); /* no new line here */
        fprintf(fout, "\"mov w0, #10\\n\\t\"\n");
        fprintf(fout, "\"cmp w0, #15\\n\\t\"\n");
    }
    
    fprintf(fout, "\"nop\\n\\t\"\n"); /* last branch target */
    fprintf(fout, ");\n");    /* close __asm__ volatile ( */
    fprintf(fout, "}\n");    /* close for loop */

    fprintf(fout, "end = clock();\n");
    fprintf(fout, "printf(\"CPU time used: %%ld\\n\", end - start);\n");

    fprintf(fout, "return 0;\n");    /* return from main */
    fprintf(fout, "}\n");    /* close main */
    fclose(fout);            /* close this file */
    
    return 0;
}