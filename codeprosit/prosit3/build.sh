#!/bin/bash
# pour exit code
set -e

# compile + lien elf
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O2 -Wall -g \
    -nostartfiles -T linker.ld \
    test.c -o test.elf

# elf ==> binaire
arm-none-eabi-objcopy -O binary test.elf test.bin

echo "test.bin compilé"
