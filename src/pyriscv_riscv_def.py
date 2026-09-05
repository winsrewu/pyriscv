from pyriscv_types import *


class PYRSISCV_CODECLASS(PyRiscvEnum):
    BASE = 0b11


class PYRSISCV_OPCODE(PyRiscvEnum):
    LUI = 0b01101
    AUIPC = 0b00101
    JAL = 0b11011
    JALR = 0b11001
    BRANCH = 0b11000
    OP_IMM = 0b00100
    OP = 0b01100
    LOAD = 0b00000
    STORE = 0b01000
    FENCE = 0b00011


class PYRSISCV_FUNCT3_OP_IMM_OP(PyRiscvEnum):
    ADD = 0b000
    SUB = 0b1000
    SLL = 0b001
    SRL = 0b101
    SRA = 0b1101
    SLT = 0b010
    SLTU = 0b011
    XOR = 0b100
    OR = 0b110
    AND = 0b111


class PYRSISCV_FUNCT3_OP_M(PyRiscvEnum):
    MUL = 0b000
    MULH = 0b001
    MULHU = 0b011
    MULHSU = 0b010
    DIV = 0b100
    DIVU = 0b101
    REM = 0b110
    REMU = 0b111


class PYRSISCV_FUNCT3_BRANCH(PyRiscvEnum):
    BEQ = 0b000
    BNE = 0b001
    BLT = 0b100
    BGE = 0b101
    BLTU = 0b110
    BGEU = 0b111


class PYRSISCV_FUNCT3_LOAD_STORE(PyRiscvEnum):
    B = 0b000
    H = 0b001
    W = 0b010
    BU = 0b100
    HU = 0b101


class PYRSISCV_ECALL_NUMBER(PyRiscvEnum):
    EXIT = 93
    WRITE = 64
    READ = 63
    DUMP = 1025
    # Standardized screen (optional device, see src/pyscreen.py).
    # a0 = word-aligned pointer to int[192*168] in guest data memory,
    # one 32-bit word per pixel: 0 = black, nonzero = white.  Screen
    # geometry is fixed at compile time on both sides.  The ecall is a
    # no-op when no display device is attached.
    SCR_DRAW = 2000
    # Game tick clock (1 game tick = 1/20 s = 50 ms).  GT_GET: a0 = the
    # current game tick counter.  GT_WAIT: pause execution until the
    # counter moves to the next game tick.  Implemented by the emulator
    # core, independent of the (optional) screen device.
    GT_GET = 2001
    GT_WAIT = 2002
    # KEY_GET: a0 = key number -> a0 = 1 if that key is held down.  Keys
    # have no built-in meaning; the launch configuration maps each number
    # to a real input key (pyriscv.py --key <n>=<name>, see pyscreen.py).
    # Returns 0 for unregistered numbers or when no window is attached.
    KEY_GET = 2003
