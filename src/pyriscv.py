import time

from pymem import PyMEM

from pyriscv_regs import PyRiscvRegs
from pyriscv_types import *
from pyriscv_riscv_def import *
from pyriscv_operator import *
from pyriscv_stat import *


class PyRiscv:
    def __init__(self, dmem, reset_vec=0, bw=32, input_buffer=""):
        self._dmem = dmem
        self._pc = reset_vec
        self._regs = PyRiscvRegs(32, bw)
        self._operator = PyRiscvOperator(bw)
        self._bw = bw
        self.input_buffer = input_buffer
        # Optional display device (see pyscreen.py).  None keeps the
        # emulator headless: the SCR_DRAW ecall is then a no-op.
        self._screen = None
        # Game tick clock: 1 tick = 1/20 s, counted from emulator start.
        self._gt_base = time.monotonic()

    def set_screen(self, screen):
        self._screen = screen

    def gt_now(self):
        """Current game tick (1/20 s since the emulator started)."""
        return int((time.monotonic() - self._gt_base) * 20)

    def gt_wait(self):
        """Pause execution until the game tick counter moves to the next tick."""
        cur = self.gt_now()
        while self.gt_now() == cur:
            nxt = self._gt_base + (cur + 1) / 20.0
            time.sleep(max(0.0, nxt - time.monotonic()))

    def key_down(self, n):
        """1 if key number n is held down; needs a windowed screen device."""
        if self._screen is not None and hasattr(self._screen, "key_down"):
            return self._screen.key_down(n)
        return 0

    def dump(self, filename):
        with open(filename, "w") as f:
            f.write(hex(PyRiscvOperator(32).unsigned(self._pc) + 4) + "\n")
            for i in range(0, 32):
                f.write(hex(PyRiscvOperator(32).unsigned(self._regs[i])) + "\n")
            self._dmem.dump(f)

    def run(self):
        self._exit = False
        while not self._exit:
            add_count()
            inst = self.fetch(self._pc)
            decode_map = self.decode(inst)

            # print(f"{hex(PyRiscvOperator(32).unsigned(self._pc))}")
            # print(f"{hex(PyRiscvOperator(32).unsigned(self._pc))} : {hex(inst._d)} ({bin(inst._d)})")
            # print(f"PC={PyRiscvOperator(32).unsigned(self._pc)}")
            # print("Registers: " + str(self._regs))
            # print("Registers=" + self._regs.to_dict_str())

            try:
                self.exec(decode_map)
            except Exception as e:
                print(
                    f"Instruction: {bin(inst._d)}, PC: {hex(PyRiscvOperator(32).unsigned(self._pc))}"
                )

                raise e

            # print("\n-----------\n")

        print(
            f"Ran {get_stat()["count"]} instructions. Bit ops: {get_stat()['bitops']}, Arithmetic ops: {get_stat()['arithmeticops']}"
        )
        print(f"Switch chance: {get_stat()['switch_chance']}")
        print(f"Arithmetic count: {get_arithmetic_count()}")

    def fetch(self, pc):
        return PyRiscvLogic(
            self._dmem[pc]
            + (self._dmem[pc + 1] << 8)
            + (self._dmem[pc + 2] << 16)
            + (self._dmem[pc + 3] << 24)
        )

    def decode(self, w):
        decode_map = PyRiscvStruct()
        decode_map.CODECLASS = PYRSISCV_CODECLASS.FV(w[1:0])
        decode_map.OPCODE = PYRSISCV_OPCODE.FV(w[6:2])
        decode_map.FUNCT3_OP_IMM_OP = PYRSISCV_FUNCT3_OP_IMM_OP.FV(w[14:12])
        decode_map.FUNCT3_OP_M = PYRSISCV_FUNCT3_OP_M.FV(w[14:12])
        decode_map.FUNCT3_BRANCH = PYRSISCV_FUNCT3_BRANCH.FV(w[14:12])
        decode_map.FUNCT3_LOAD_STORE = PYRSISCV_FUNCT3_LOAD_STORE.FV(w[14:12])
        decode_map.FUNCT7 = w[31:25]
        decode_map.RD = w[11:7]
        decode_map.RS1 = w[19:15]
        decode_map.RS2 = w[24:20]
        decode_map.IMMJ = PyRiscvOperator(21).signed(
            (w[30:21] << 1) | (w[20] << 11) | (w[19:12] << 12) | (w[31] << 20)
        )
        decode_map.IMMB = PyRiscvOperator(13).signed(
            (w[11:8] << 1) | (w[30:25] << 5) | (w[7] << 11) | (w[31] << 12)
        )
        decode_map.IMMI = PyRiscvOperator(12).signed(w[31:20])
        decode_map.IMMS = PyRiscvOperator(12).signed(w[11:7] + (w[31:25] << 5))
        decode_map.IMMU = w[31:12] << 12
        decode_map.EBREAK = int(w) == 0x00100073
        decode_map.ECALL = int(w) == 0x00000073

        # FUNCT7
        if decode_map.FUNCT7 == 0x20 and decode_map.OPCODE == PYRSISCV_OPCODE.OP:
            if decode_map.FUNCT3_OP_IMM_OP == PYRSISCV_FUNCT3_OP_IMM_OP.ADD:
                decode_map.FUNCT3_OP_IMM_OP = PYRSISCV_FUNCT3_OP_IMM_OP.SUB
            elif decode_map.FUNCT3_OP_IMM_OP == PYRSISCV_FUNCT3_OP_IMM_OP.SRL:
                decode_map.FUNCT3_OP_IMM_OP = PYRSISCV_FUNCT3_OP_IMM_OP.SRA
        elif decode_map.FUNCT7 == 0x20 and decode_map.OPCODE == PYRSISCV_OPCODE.OP_IMM:
            if decode_map.FUNCT3_OP_IMM_OP == PYRSISCV_FUNCT3_OP_IMM_OP.SRL:
                decode_map.FUNCT3_OP_IMM_OP = PYRSISCV_FUNCT3_OP_IMM_OP.SRA

        # M extension
        if decode_map.FUNCT7 == 0x01 and decode_map.OPCODE == PYRSISCV_OPCODE.OP:
            decode_map.FUNCT3_OP_IMM_OP = decode_map.FUNCT3_OP_M
        return decode_map

    def exec(self, decode_map):
        if decode_map.CODECLASS != PYRSISCV_CODECLASS.BASE:
            raise Exception("Invalid code class")

        if decode_map.OPCODE == PYRSISCV_OPCODE.JAL:
            self._regs[decode_map.RD] = self._pc + 4
            self._pc += decode_map.IMMJ

        elif decode_map.OPCODE == PYRSISCV_OPCODE.JALR:
            t = self._pc + 4
            self._pc = ((decode_map.IMMI + self._regs[decode_map.RS1]) | 0x1) - 1
            self._regs[decode_map.RD] = t

        elif decode_map.OPCODE == PYRSISCV_OPCODE.BRANCH:
            if self._operator(
                decode_map.FUNCT3_BRANCH,
                self._regs[decode_map.RS1],
                self._regs[decode_map.RS2],
            ):
                self._pc += decode_map.IMMB
            else:
                self._pc += 4

        elif decode_map.OPCODE == PYRSISCV_OPCODE.OP_IMM:
            self._regs[decode_map.RD] = self._operator(
                decode_map.FUNCT3_OP_IMM_OP, self._regs[decode_map.RS1], decode_map.IMMI
            )
            self._pc += 4

        elif decode_map.OPCODE == PYRSISCV_OPCODE.OP:
            self._regs[decode_map.RD] = self._operator(
                decode_map.FUNCT3_OP_IMM_OP,
                self._regs[decode_map.RS1],
                self._regs[decode_map.RS2],
            )
            self._pc += 4

        elif decode_map.OPCODE == PYRSISCV_OPCODE.LUI:
            self._regs[decode_map.RD] = decode_map.IMMU
            self._pc += 4

        elif decode_map.OPCODE == PYRSISCV_OPCODE.AUIPC:
            self._regs[decode_map.RD] = self._pc + decode_map.IMMU
            self._pc += 4

        elif decode_map.OPCODE == PYRSISCV_OPCODE.LOAD:
            dmem_base = self._regs[decode_map.RS1] + decode_map.IMMI
            if decode_map.FUNCT3_LOAD_STORE == PYRSISCV_FUNCT3_LOAD_STORE.W:
                self._regs[decode_map.RD] = PyRiscvOperator(self._bw).signed(
                    self._dmem[dmem_base]
                    + (self._dmem[dmem_base + 1] << 8)
                    + (self._dmem[dmem_base + 2] << 16)
                    + (self._dmem[dmem_base + 3] << 24)
                )
            elif decode_map.FUNCT3_LOAD_STORE == PYRSISCV_FUNCT3_LOAD_STORE.H:
                self._regs[decode_map.RD] = PyRiscvOperator(16).signed(
                    self._dmem[dmem_base] + (self._dmem[dmem_base + 1] << 8)
                )
            elif decode_map.FUNCT3_LOAD_STORE == PYRSISCV_FUNCT3_LOAD_STORE.HU:
                self._regs[decode_map.RD] = self._dmem[dmem_base] + (
                    self._dmem[dmem_base + 1] << 8
                )
            elif decode_map.FUNCT3_LOAD_STORE == PYRSISCV_FUNCT3_LOAD_STORE.B:
                self._regs[decode_map.RD] = PyRiscvOperator(8).signed(
                    self._dmem[dmem_base]
                )
            elif decode_map.FUNCT3_LOAD_STORE == PYRSISCV_FUNCT3_LOAD_STORE.BU:
                self._regs[decode_map.RD] = self._dmem[dmem_base]
            else:
                raise Exception("Invalid load instruction")

            self._pc += 4

        elif decode_map.OPCODE == PYRSISCV_OPCODE.STORE:
            dmem_base = self._regs[decode_map.RS1] + decode_map.IMMS
            dmem_data = PyRiscvOperator(self._bw).unsigned(self._regs[decode_map.RS2])
            if decode_map.FUNCT3_LOAD_STORE == PYRSISCV_FUNCT3_LOAD_STORE.W:
                self._dmem[dmem_base] = dmem_data & 0xFF
                self._dmem[dmem_base + 1] = (dmem_data & 0xFF00) >> 8
                self._dmem[dmem_base + 2] = (dmem_data & 0xFF0000) >> 16
                self._dmem[dmem_base + 3] = (dmem_data & 0xFF000000) >> 24
            elif decode_map.FUNCT3_LOAD_STORE == PYRSISCV_FUNCT3_LOAD_STORE.H:
                self._dmem[dmem_base] = dmem_data & 0xFF
                self._dmem[dmem_base + 1] = (dmem_data & 0xFF00) >> 8
            elif decode_map.FUNCT3_LOAD_STORE == PYRSISCV_FUNCT3_LOAD_STORE.B:
                self._dmem[dmem_base] = dmem_data & 0xFF
            else:
                raise Exception("Invalid store instruction")

            self._pc += 4

        elif decode_map.OPCODE == PYRSISCV_OPCODE.FENCE:
            self._pc += 4

        elif decode_map.EBREAK:
            self._pc += 4

        elif decode_map.ECALL:
            ecall_num = PYRSISCV_ECALL_NUMBER.FV(self._regs[17])

            if ecall_num == PYRSISCV_ECALL_NUMBER.EXIT:
                print("Exiting with code " + str(self._regs[10]))
                self._exit = True
            elif ecall_num == PYRSISCV_ECALL_NUMBER.WRITE:
                # print(f"Writing {self._regs[12]} bytes to file descriptor {self._regs[11]}")

                if self._regs[10] == 1:
                    addr_tmp = self._regs[11]
                    for i in range(self._regs[12]):
                        print(chr(self._dmem[addr_tmp + i]), end="")

                    # set return value to length of written data
                    self._regs[10] = self._regs[12]
                else:
                    raise Exception(
                        "Invalid file descriptor in write ecall", self._regs[10]
                    )

            elif ecall_num == PYRSISCV_ECALL_NUMBER.READ:
                if self._regs[10] == 0:
                    # print(hex(PyRiscvOperator(32).unsigned(self._regs[11])))
                    addr_tmp = self._regs[11]
                    read_count = 0

                    if len(self.input_buffer) == 0:
                        self.input_buffer = (
                            input(
                                f"Enter input for read ecall (max {self._regs[12]} bytes): "
                            )
                            + "\n"
                        )

                    for i in range(self._regs[12]):
                        if len(self.input_buffer) == 0:
                            break
                        else:
                            self._dmem[addr_tmp + i] = ord(self.input_buffer[0])
                            self.input_buffer = self.input_buffer[1:]
                            read_count += 1

                    # set return value to length of read data
                    self._regs[10] = read_count
                else:
                    raise Exception(
                        "Invalid file descriptor in read ecall", self._regs[10]
                    )

            elif ecall_num == PYRSISCV_ECALL_NUMBER.DUMP:
                print("Dumping memory...")
                self.dump("dump.txt")
                print("Done.")

            elif ecall_num == PYRSISCV_ECALL_NUMBER.SCR_DRAW:
                # Standardized screen draw.  a0 = fb pointer; the screen
                # geometry is fixed (see pyscreen.py).  Optional device:
                # without one attached this is a no-op.  draw() returns
                # False when the window is closed (or --frames is reached),
                # which stops the emulator.
                if self._screen is not None:
                    if not self._screen.draw(self._dmem, self._regs[10]):
                        self._exit = True

            elif ecall_num == PYRSISCV_ECALL_NUMBER.GT_GET:
                # current game tick (1/20 s clock)
                self._regs[10] = self.gt_now()

            elif ecall_num == PYRSISCV_ECALL_NUMBER.GT_WAIT:
                # pause until the next game tick
                self.gt_wait()

            elif ecall_num == PYRSISCV_ECALL_NUMBER.KEY_GET:
                # a0 = key number -> held flag (registered at launch)
                self._regs[10] = self.key_down(self._regs[10])

            else:
                raise Exception("Invalid ecall number", self._regs[17])

            self._pc += 4

        else:
            raise Exception("Invalid instruction")


if __name__ == "__main__":
    import sys

    args = sys.argv[1:]
    if not args:
        raise SystemExit("usage: pyriscv.py <app.mem> [input] [--screen] "
                         "[--dump out.ppm] [--frames N] [--scale N] "
                         "[--width W] [--height H] [--key <n>=<key>]")
    dmem = PyMEM(args[0])
    args = args[1:]

    input_buffer = ""
    if args and not args[0].startswith("--"):
        input_buffer = args.pop(0)

    screen_mode = None  # None | "window" | "headless"
    dump_path = None
    max_frames = 0
    scale = 4
    # Screen size is a launch parameter (default matches the compile-time
    # geometry of app/gfx-saver and the MC wall).  Presenting a different
    # size only makes sense together with a guest that draws that size:
    # change app/gfx-saver/main.c (SCR_W/SCR_H), app/c-common/link.ld
    # (SCREENFB LENGTH, only when the buffer outgrows it) and riscvmc2
    # src/python/config.py (screen_width/screen_height) together.
    scr_w, scr_h = 192, 168
    # Key number -> input key registration (KEY_GET ecall).  Values are
    # resolved to pygame keys when the window opens (single char like "w",
    # or a name like "up"/"space"); see pyscreen.py.
    keymap = {}
    while args:
        a = args.pop(0)
        if a == "--screen":
            screen_mode = "window"
        elif a == "--dump":
            if not args:
                raise SystemExit("--dump needs a file path")
            screen_mode = "headless"
            dump_path = args.pop(0)
        elif a == "--frames":
            max_frames = int(args.pop(0))
        elif a == "--scale":
            scale = int(args.pop(0))
        elif a == "--width":
            scr_w = int(args.pop(0))
        elif a == "--height":
            scr_h = int(args.pop(0))
        elif a == "--key":
            if not args:
                raise SystemExit("--key needs <n>=<name>")
            pair = args.pop(0)
            n, _, name = pair.partition("=")
            keymap[int(n)] = name
        else:
            raise SystemExit(f"Unknown argument: {a}")

    emulator = PyRiscv(dmem, reset_vec=0x00000000, input_buffer=input_buffer)
    if screen_mode is not None:
        # Optional display module; imported lazily so the core emulator
        # never depends on pygame unless a screen is requested.
        from pyscreen import Screen

        emulator.set_screen(
            Screen(mode=screen_mode, dump_path=dump_path,
                   max_frames=max_frames, scale=scale,
                   width=scr_w, height=scr_h, keymap=keymap)
        )
    emulator.run()
