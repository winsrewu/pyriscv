set -e

riscv32-unknown-elf-g++ -g -O3 -std=c++20 -static -specs=nosys.specs -march=rv32i -mabi=ilp32 -DNDEBUG -T ../c-common/gptype_b.ld -o app.elf ../c-common/vectors.S ../c-common/syscalls.c main.cpp -lc -lgcc
riscv32-unknown-elf-objcopy -F verilog app.elf app.mem
riscv32-unknown-elf-objdump -S -d app.elf > app.lst