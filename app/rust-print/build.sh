set -e

cargo build --release
riscv32-unknown-elf-objcopy -O verilog target/riscv32i-unknown-none-elf/release/rust-print app.mem