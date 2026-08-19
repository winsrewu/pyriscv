#!/bin/sh
# Build Minecraft-1960 for the pyriscv emulator (RV32IM, bare-metal).
# Produces app.mem alongside app.elf and app.lst.
#
#   ./build_riscv.sh
#   python3 ../../src/pyriscv.py app.mem
set -e

GCC=riscv32-unknown-elf-gcc
OBJCOPY=riscv32-unknown-elf-objcopy
OBJDUMP=riscv32-unknown-elf-objdump

CFLAGS="-O3 -flto -g -static -mrelax -specs=nosys.specs -march=rv32im -mabi=ilp32 -DPY_RISCV_GFX"

$GCC $CFLAGS -T ../c-common/link.ld -o app.elf \
    ../c-common/vectors.S ../c-common/syscalls.c \
    main.c game.c world.c craft.c render.c display_riscv.c \
    -lc -lgcc

$OBJCOPY -O verilog app.elf app.mem
$OBJDUMP -S -d app.elf > app.lst

echo "built app.mem ($(wc -c < app.mem) bytes)"
