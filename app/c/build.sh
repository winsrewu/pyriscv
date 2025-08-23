set -e

riscv32-unknown-elf-gcc -static -march=rv32i -mabi=ilp32 -T link.ld -o app.elf vectors.S syscalls.c main.c -lc -lgcc 
riscv32-unknown-elf-objcopy -F verilog app.elf app.mem
riscv32-unknown-elf-objdump -S -d app.elf > app.lst