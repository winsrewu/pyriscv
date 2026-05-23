set -e

riscv32-unknown-elf-gcc -g -static -march=rv32im -mabi=ilp32 main.c -o main.out

riscv32-unknown-elf-gcc -g -static -mrelax -specs=nosys.specs -march=rv32im -mabi=ilp32 -T ../c-common/link.ld -o app.elf ../c-common/vectors.S ../c-common/syscalls.c main.c -lc -lgcc
riscv32-unknown-elf-objcopy -O verilog app.elf app.mem
riscv32-unknown-elf-objdump -S -d app.elf > app.lst