#!/bin/bash

# filepath: /home/yoga/RPi_BPU_RE/BTB/Size/test/gen.sh

iterations=1000000
distances=(32 64 128 256)
branches=(512 1024 2048 4096)

for dist in "${distances[@]}"; do
    for branch in "${branches[@]}"; do
        echo "Running gencode with iterations=$iterations, dist=$dist, branches=$branch"
        ./gencode $iterations $dist $branch
    done
done