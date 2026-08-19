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
    # Display device (see src/pydisplay.py and app/minecraft-1960-c/display_riscv.c).
    # The hardware clock ticks at 20 Hz (50 ms granularity).
    SCR_CLEAR = 2000
    SCR_SET = 2001
    DSP_INIT = 2002
    DSP_PRESENT = 2003
    DSP_SYNC = 2004
    DSP_MS = 2005
    DSP_SLEEP = 2006
    INPUT_POLL = 2007
    KEY_GET = 2008
    KEY_PRESSED = 2009
    # High-level drawing primitives (rendered natively in src/pygfx.py)
    SCR_HLINE = 2010
    SCR_VLINE = 2011
    SCR_RECT = 2012
    SCR_FILL = 2013
    SCR_CHAR = 2014
    SCR_TEXT = 2015
    SCR_NUM = 2016
    SCR_ICON = 2017
    SCR_SPECKLE = 2018
    SCR_HEART = 2019
    SCR_FLUID = 2020
    SCR_WORLD = 2021
