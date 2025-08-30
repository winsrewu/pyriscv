set -e

riscv32-unknown-elf-g++ -g -static -O3 -specs=nosys.specs -march=rv32i -mabi=ilp32 -T ../c-common/link.ld -o app.elf ../c-common/vectors.S ../c-common/syscalls.c main.cpp -lc -lgcc
riscv32-unknown-elf-objcopy -F verilog app.elf app.mem
riscv32-unknown-elf-objdump -S -d app.elf > app.lst