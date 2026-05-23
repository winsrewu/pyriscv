import random

u32 = lambda v: v & 0xFFFFFFFF
s32 = lambda v: v if v < 0x80000000 else v - 0x100000000

# RV32I ALU/Shift (excludes load/store, ecall, ebreak)
I_OPS = {
    "add": lambda a, b: u32(a + b),
    "sub": lambda a, b: u32(a - b),
    "and": lambda a, b: u32(a & b),
    "or": lambda a, b: u32(a | b),
    "xor": lambda a, b: u32(a ^ b),
    "sll": lambda a, b: u32(a << (b & 0x1F)),
    "srl": lambda a, b: u32(u32(a) >> (b & 0x1F)),
    "sra": lambda a, b: u32(s32(a) >> (b & 0x1F)),
}


# RV32M (matches spec: truncation toward zero, div-by-zero=-1, rem-by-zero=dividend)
def _div(a, b):
    sa, sb = s32(a), s32(b)
    if sb == 0:
        return u32(-1)
    if sa == -0x80000000 and sb == -1:
        return u32(sa)
    return u32(int(sa / sb))


def _rem(a, b):
    sa, sb = s32(a), s32(b)
    return u32(sa) if sb == 0 else u32(sa - sb * int(sa / sb))


M_OPS = {
    "mul": lambda a, b: u32(s32(a) * s32(b)),
    "mulh": lambda a, b: u32((s32(a) * s32(b)) >> 32),
    "mulhsu": lambda a, b: u32((s32(a) * u32(b)) >> 32),
    "mulhu": lambda a, b: u32((u32(a) * u32(b)) >> 32),
    "div": _div,
    "divu": lambda a, b: u32(-1) if u32(b) == 0 else u32(u32(a) // u32(b)),
    "rem": _rem,
    "remu": lambda a, b: u32(a) if u32(b) == 0 else u32(u32(a) % u32(b)),
}


def gen_cases():
    cases = [
        (random.randint(0, 0xFFFFFFFF), random.randint(0, 0xFFFFFFFF))
        for _ in range(500)
    ]
    cases += [
        (0, 0),
        (0xFFFFFFFF, 0xFFFFFFFF),
        (0x80000000, 0x80000000),
        (0x80000000, 0),
        (0xFFFFFFFF, 0),
        (0x7FFFFFFF, 0),
        (0x7FFFFFFF, 1),
        (0x7FFFFFFF, 2),
        (1, 0),
        (0, 1),
        (0x80000000, 0xFFFFFFFF),
        (0xAAAAAAAA, 0x55555555),
        (0x12345678, 0x87654321),
        (0x80000000, 1),
    ]
    cases += [(random.randint(0, 0xFFFFFFFF), 0) for _ in range(50)]
    return list(set(cases))


def generate_c():
    c = """#include <stdio.h>
#include <stdint.h>
#define TEST(insn, op1, op2, exp) do { \
    uint32_t res; \
    asm volatile(#insn " %0, %1, %2" : "=r"(res) : "r"(op1), "r"(op2)); \
    if (res != (exp)) { printf("FAIL: %s op1=0x%08x op2=0x%08x exp=0x%08x got=0x%08x\\n", #insn, op1, op2, (exp), res); fail++; } \
} while(0)
int main() { int fail = 0;\n"""
    for n, f in {**I_OPS, **M_OPS}.items():
        for a, b in gen_cases():
            c += f"    TEST({n}, 0x{a:08x}, 0x{b:08x}, 0x{f(a,b):08x});\n"
    return (
        c
        + '\n    if(fail) printf("FAILED: %d errors\\n", fail); else printf("PASSED\\n"); return fail; }\n'
    )


if __name__ == "__main__":
    with open("rv32im_test.c", "w") as f:
        f.write(generate_c())
    print("Generated rv32im_test.c")
