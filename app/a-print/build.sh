set -e

riscv32-unknown-elf-gcc -g -static -nostdlib -march=rv32i -mabi=ilp32 -T link.ld -o app.elf app.S
riscv32-unknown-elf-objcopy -F verilog app.elf app.mem
riscv32-unknown-elf-objdump -S -d app.elf > app.lst