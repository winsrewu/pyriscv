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
- read system call should only read from STDIN, whose fileno is 0.
- cpp is really complicated, i tried to use cpp's iostream to output, but failed (i've tested it on spike, it will try to load from 0x0, idk why). But just using g++ to compile some simple stuff is ok.
- I now know why. I missed some of the initialization code. Check them in app/cpp-stream. Now std::cin and std::cout should work.

# system calls table
| number | function | args | return |
|--------|----------|------|--------|
| 63     | read     | fd, ptr, len | number of bytes read |
| 64     | write    | fd, ptr, len | number of bytes written |
| 93     | exit     | error_code | - |