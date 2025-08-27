# pyriscv
A RISCV Emulator written in Python, supports RV32I instruction set.

Do NOT support csr instructions.

Requires:
  python3.4+
  
No other libraries!!

# run
```bash
cd app/c
./build.sh
python3 ../../src/pyriscv.py app.mem
```

# good to know
- FENCE and FENCE.I instructions do NOT have any effect on the emulator.
- EBREAK instruction do NOT have any effect on the emulator.
- for ECALL, a7 register is for passing system call number, a0-a6 registers are for passing arguments (a0 for the first argument),
return value is stored in a0 register.
- write system call should only write to STDOUT, whose fileno is 1.

# system calls table
| number | function | args | return |
|--------|----------|------|--------|
| 64     | write    | fd, ptr, len | number of bytes written |
| 93     | exit     | error_code | - |