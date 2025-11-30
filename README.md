# pyriscv
A RISC-V Emulator written in Python, supports RV32I instruction set.

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
- This project uses Black Formatter to format python code.
- The default entry point is 0x0.
- FENCE and FENCE.I instructions do NOT have any effect on the emulator.
- EBREAK instruction do NOT have any effect on the emulator.
- for ECALL, a7 register is for passing system call number, a0-a6 registers are for passing arguments (a0 for the first argument),
return value is stored in a0 register.
- write system call should only write to STDOUT, whose fileno is 1.
- read system call should only read from STDIN, whose fileno is 0.
- cpp is really complicated, i tried to use cpp's iostream to output, but failed (i've tested it on spike, it will try to load from 0x0, idk why). But just using g++ to compile some simple stuff is ok.
- I now know why. There're some problems with the linker script, and i fixed it. Now most of the code should work fine.
- Global pointer is really easy to fail, you may need to adjust it manually for different programs.
For instance, for some programs, change that line from
``__global_pointer$ = MIN(__PREINIT_ARRAY_BEGIN__ + 0x800,
MAX(__DATA_BEGIN__ + 0x800, __BSS_END__ - 0x800));``
to ``  __global_pointer$ = MIN(__SDATA_BEGIN__ + 0x800,
MAX(__DATA_BEGIN__ + 0x800, __BSS_END__ - 0x800));`` will make it work.
I made two versions of link script, you can check the difference between them through the build script of cpp-stream and cpp-stream-2. If this still doesn't work, you may modify the link script manually or just disable the global pointer feature
via ``-Wl,--no-relax-gp`` option.

# system calls table
| number | function | args | return |
|--------|----------|------|--------|
| 63     | read     | fd, ptr, len | number of bytes read |
| 64     | write    | fd, ptr, len | number of bytes written |
| 93     | exit     | error_code | - |
| 1025   | dump     | - | - |

### explanation
- calling dump system call will dump current pc,
registers, memory to a file named "dump.txt".
The first line of the file is the current pc + 4,
which means the next instruction to be executed.
The next 32 lines are the registers,
all the lines after that are the memory,
in (almost) the same format as the .mem files.