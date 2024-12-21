#!/bin/bash

# filepath: /home/yoga/RPi_BPU_RE/BTB/Size/test/run_branch.sh

file_prefix="T1I1mN"

distances=(32 64 128 256)
branches=(512 1024 2048 4096)

for dist in "${distances[@]}"; do
    for branch in "${branches[@]}"; do
        file_name="${file_prefix}${dist}B${branch}.c"
        gcc -o "${file_name%.c}" "$file_name"
        
        if [ $? -ne 0 ]; then
            echo "Compilation failed for $file_name"
            continue
        fi
        
        for i in {1..3}; do
            echo "Running ${file_name%.c}, iteration $i"
            ./"${file_name%.c}"
        done

        rm -f "${file_name%.c}"
    done
done