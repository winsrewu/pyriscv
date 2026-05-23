from pyriscv_riscv_def import *
from pyriscv_stat import *
import operator

import math


class PyRiscvOperator:

    def __init__(self, bw):
        self._bw = bw

        self._mask = (1 << bw) - 1
        self._tmin = -(1 << (bw - 1))  # Minimum signed value
        self._tmax = (1 << (bw - 1)) - 1  # Maximum signed value

        self._exec_map = {
            PYRSISCV_FUNCT3_OP_IMM_OP.ADD: self.add,
            PYRSISCV_FUNCT3_OP_IMM_OP.SUB: self.sub,
            PYRSISCV_FUNCT3_OP_IMM_OP.XOR: operator.xor,
            PYRSISCV_FUNCT3_OP_IMM_OP.OR: operator.or_,
            PYRSISCV_FUNCT3_OP_IMM_OP.AND: operator.and_,
            PYRSISCV_FUNCT3_OP_IMM_OP.SLT: self.slt,
            PYRSISCV_FUNCT3_OP_IMM_OP.SLTU: self.sltu,
            PYRSISCV_FUNCT3_OP_IMM_OP.SLL: self.sll,
            PYRSISCV_FUNCT3_OP_IMM_OP.SRL: self.srl,
            PYRSISCV_FUNCT3_OP_IMM_OP.SRA: self.sra,
            PYRSISCV_FUNCT3_OP_M.MUL: self.mul,
            PYRSISCV_FUNCT3_OP_M.MULH: self.mulh,
            PYRSISCV_FUNCT3_OP_M.MULHSU: self.mulhsu,
            PYRSISCV_FUNCT3_OP_M.MULHU: self.mulhu,
            PYRSISCV_FUNCT3_OP_M.DIV: self.div,
            PYRSISCV_FUNCT3_OP_M.DIVU: self.divu,
            PYRSISCV_FUNCT3_OP_M.REM: self.rem,
            PYRSISCV_FUNCT3_OP_M.REMU: self.remu,
            PYRSISCV_FUNCT3_BRANCH.BEQ: self.beq,
            PYRSISCV_FUNCT3_BRANCH.BNE: self.bne,
            PYRSISCV_FUNCT3_BRANCH.BLT: self.blt,
            PYRSISCV_FUNCT3_BRANCH.BLTU: self.bltu,
            PYRSISCV_FUNCT3_BRANCH.BGE: self.bge,
            PYRSISCV_FUNCT3_BRANCH.BGEU: self.bgeu,
        }

    def __call__(self, funct3, *args):
        if (
            funct3 == PYRSISCV_FUNCT3_OP_IMM_OP.XOR
            or funct3 == PYRSISCV_FUNCT3_OP_IMM_OP.OR
            or funct3 == PYRSISCV_FUNCT3_OP_IMM_OP.AND
        ):
            add_bitops()
        return self._exec_map[funct3](*args)

    def signed(self, x):
        bw = self._bw
        mask = (1 << bw) - 1
        val = x & mask
        half = 1 << (bw - 1)
        return val - (1 << bw) if val >= half else val

    def unsigned(self, x):
        bw = self._bw
        mask = (1 << bw) - 1
        return x & mask

    def limit(self, x):
        bw = self._bw
        mask = (1 << bw) - 1
        truncated = x & mask
        return self.signed(truncated)

    # I have no idea what's the difference between helper functions above and below
    # They are AI generated and I don't want to care
    # Because this project itself sucks
    # It's only a reference model...
    def _to_signed(self, x: int) -> int:
        return self.signed(x)

    def _to_unsigned(self, x: int) -> int:
        """Convert signed integer to unsigned bit pattern."""
        return x & self._mask

    def _truncate(self, x: int) -> int:
        """Truncate value to target bitwidth, return as signed integer."""
        return self._to_signed(x & self._mask)

    def slt(self, a, b):
        add_arithmeticops("compare")
        return 1 if self.signed(a) < self.signed(b) else 0

    def sltu(self, a, b):
        add_arithmeticops("compare")
        return 1 if self.unsigned(a) < self.unsigned(b) else 0

    def add(self, a, b):
        add_arithmeticops("add")
        return self.limit(a + b)

    def sub(self, a, b):
        add_arithmeticops("sub")
        return self.limit(a - b)

    def sll(self, a, b):
        add_bitops()
        b = self.unsigned(b)
        shamt = b & 0x1F
        a = self.unsigned(a)
        return self.limit(a << shamt)

    def sra(self, a, b):
        add_bitops()
        b = self.unsigned(b)
        shamt = b & 0x1F
        a = self.signed(a)
        return self.limit(a >> shamt)

    def srl(self, a, b):
        add_bitops()
        b = self.unsigned(b)
        shamt = b & 0x1F
        a = self.unsigned(a)
        return self.limit(a >> shamt)

    def beq(self, a, b):
        add_arithmeticops("compare")
        return self.signed(a) == self.signed(b)

    def bne(self, a, b):
        add_arithmeticops("compare")
        return self.signed(a) != self.signed(b)

    def blt(self, a, b):
        add_arithmeticops("compare")
        return self.signed(a) < self.signed(b)

    def bltu(self, a, b):
        add_arithmeticops("compare")
        return self.unsigned(a) < self.unsigned(b)

    def bge(self, a, b):
        add_arithmeticops("compare")
        return self.signed(a) >= self.signed(b)

    def bgeu(self, a, b):
        add_arithmeticops("compare")
        return self.unsigned(a) >= self.unsigned(b)

    def mul(self, a: int, b: int) -> int:
        """
        MUL: Signed multiply, return lower 32/64 bits.
        Result = (a_signed * b_signed)[BW-1:0]
        """
        add_arithmeticops("mul")
        a_s, b_s = self._to_signed(a), self._to_signed(b)
        product = a_s * b_s
        # Keep only lower BW bits, then interpret as signed
        return self._truncate(product)

    def mulh(self, a: int, b: int) -> int:
        """
        MULH: Signed x Signed multiply, return upper 32/64 bits.
        Result = (a_signed * b_signed)[2*BW-1:BW]
        """
        add_arithmeticops("mul")
        a_s, b_s = self._to_signed(a), self._to_signed(b)
        product = a_s * b_s
        # Extract upper BW bits with proper sign handling
        high = product >> self._bw
        return self._truncate(high)

    def mulhu(self, a: int, b: int) -> int:
        """
        MULHU: Unsigned x Unsigned multiply, return upper 32/64 bits.
        Result = (a_unsigned * b_unsigned)[2*BW-1:BW]
        """
        add_arithmeticops("mul")
        a_u, b_u = self._to_unsigned(a), self._to_unsigned(b)
        product = a_u * b_u
        high = product >> self._bw
        # Result is unsigned, but we return as signed bit pattern per RISC-V convention
        return self._truncate(high)

    def mulhsu(self, a: int, b: int) -> int:
        """
        MULHSU: Signed x Unsigned multiply, return upper 32/64 bits.
        Result = (a_signed * b_unsigned)[2*BW-1:BW]
        """
        add_arithmeticops("mul")
        a_s = self._to_signed(a)
        b_u = self._to_unsigned(b)
        product = a_s * b_u
        high = product >> self._bw
        return self._truncate(high)

    def _div_rem_core(self, a: int, b: int, signed: bool) -> tuple[int, int]:
        if signed:
            a_val, b_val = self._to_signed(a), self._to_signed(b)

            # Edge case: TMIN / -1 overflows, return TMIN per spec
            if b_val == -1 and a_val == self._tmin:
                return self._tmin, 0

            # Edge case: Division by zero, return -1 (all 1s) and dividend
            if b_val == 0:
                return self._mask, a_val  # -1 as bit pattern, remainder = dividend

            # RISC-V requires truncation toward zero (not floor division)
            q = math.trunc(a_val / b_val)
            r = a_val - q * b_val

        else:
            a_val, b_val = self._to_unsigned(a), self._to_unsigned(b)

            # Edge case: Division by zero for unsigned
            if b_val == 0:
                return self._mask, a_val  # all 1s, remainder = dividend

            q = a_val // b_val  # Unsigned division is same for trunc/floor
            r = a_val - q * b_val

        # Convert results back to signed bit patterns for return
        return self._truncate(q), self._truncate(r)

    def div(self, a: int, b: int) -> int:
        """
        DIV: Signed division, truncates toward zero.
        Returns quotient as signed bit pattern.
        """
        add_arithmeticops("div")
        return self._div_rem_core(a, b, signed=True)[0]

    def divu(self, a: int, b: int) -> int:
        """
        DIVU: Unsigned division.
        Returns quotient as signed bit pattern (raw bits).
        """
        add_arithmeticops("div")
        return self._div_rem_core(a, b, signed=False)[0]

    def rem(self, a: int, b: int) -> int:
        """
        REM: Signed remainder. Sign of result matches dividend (a).
        Returns remainder as signed bit pattern.
        """
        add_arithmeticops("div")
        return self._div_rem_core(a, b, signed=True)[1]

    def remu(self, a: int, b: int) -> int:
        """
        REMU: Unsigned remainder.
        Returns remainder as signed bit pattern (raw bits).
        """
        add_arithmeticops("div")
        return self._div_rem_core(a, b, signed=False)[1]
