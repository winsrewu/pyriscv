# pyriscv
A RISC-V U-mode Emulator written in Python, supports RV32IM instruction set.

Do NOT support csr instructions or privilege architecture.

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
- This project SUCKS. I am talking to you origin author.
- This project uses Black Formatter to format python code.
- The default entry point is 0x0.
- FENCE and FENCE.I instructions do NOT have any effect on the emulator.
- EBREAK instruction do NOT have any effect on the emulator.
- for ECALL, a7 register is for passing system call number, a0-a6 registers are for passing arguments (a0 for the first argument),
return value is stored in a0 register.
- write system call should only write to STDOUT, whose fileno is 1.
- read system call should only read from STDIN, whose fileno is 0.
- cpp is really complicated, i tried to use cpp's iostream to output, but failed (i've tested it on spike, it will try to load from 0x0, idk why). But just using g++ to compile some simple stuff is ok.
- Global pointer's position is modified compared to the origin linker script.
- There two sector in the memory, one is text, unmodifiable. And another on is data, modifiable but not executable.

# system calls table
| number | function | args | return |
|--------|----------|------|--------|
| 63     | read     | fd, ptr, len | number of bytes read |
| 64     | write    | fd, ptr, len | number of bytes written |
| 93     | exit     | error_code | - |
| 1025   | dump     | - | - |
| 2000   | scr_draw | fb ptr | - |
| 2001   | gt_get   | - | current game tick (1/20 s clock) |
| 2002   | gt_wait  | - | pause until the next game tick |
| 2003   | key_get  | key number | 1 if that key is held down |

### screen (optional module)
- The screen is an **optional** display device (src/pyscreen.py): without
  it the emulator stays headless and ecall 2000 is a no-op.  pygame is
  only imported when the screen is requested.
- Geometry is a **launch parameter**: `--width W --height H` (defaults
  192x168, matching the compile-time geometry of app/gfx-saver/main.c and
  the riscvmc2 MC wall).  Row-major, origin at the top-left.  Presenting
  another size only makes sense together with a guest built for that size
  (guest fb array in app/gfx-saver/main.c, the .screenfb window in
  app/c-common/link.ld, and riscvmc2 plugin/screen_gen.py are all
  compile-time sized).
- The guest presents an `int[width * height]` framebuffer in its data
  memory with the single ecall 2000: a0 = word-aligned fb pointer, one
  32-bit word per pixel, 0 = black, nonzero = white.  A misaligned pointer
  raises an error.  Placement is outside this call.
- Game tick clock (2001/2002) is provided by the emulator core and works
  headless too: 1 game tick = 1/20 s.  `gt_wait` blocks (real time) until
  the tick counter moves on, so guests can pace themselves.
- Keys (2003) are **numbers without built-in meaning**: register each
  number to a real key at launch with repeatable `--key <n>=<key>`, e.g.
  `--key 0=w --key 1=s`.  Accepted values: a single character (`"w"`) or
  a pygame key name (`"up"`, `"space"`, ...).  Unregistered numbers (or
  headless runs) read 0.
- Usage:
  - `python3 src/pyriscv.py app/gfx-saver/app.mem --screen --key 0=w --key 1=s`
    (pygame window; gfx-saver: key 0 / w = larger target interval =
    slower, key 1 / s = smaller = faster)
  - `python3 src/pyriscv.py app/gfx-saver/app.mem --frames 1 --dump out.ppm`
    (headless; writes the last frame as .ppm, or .pbm if the path ends
    with .pbm)
  - extra flags: `--frames N` (stop after N draws), `--scale N` (window
    zoom).  The window closes on ESC / close button; headless mode needs
    `--frames`.

### explanation
- calling dump system call will dump current pc,
registers, memory to a file named "dump.txt".
The first line of the file is the current pc + 4,
which means the next instruction to be executed.
The next 32 lines are the registers,
all the lines after that are the memory,
in (almost) the same format as the .mem files.