from memory_tracer import MEM_TRACER
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

            MEM_TRACER.set_running_inst(self._pc, inst)

            # print(f"{hex(PyRiscvOperator(32).unsigned(self._pc))}")
            # print(f"{hex(PyRiscvOperator(32).unsigned(self._pc))} : {hex(inst._d)} ({bin(inst._d)})")
            # print(f"PC={hex(PyRiscvOperator(32).unsigned(self._pc))}")
            # print("Registers: " + str(self._regs))
            # print("Registers=" + self._regs.to_dict_str())

            try:
                self.exec(decode_map)
            except Exception as e:
                print(
                    f"Instruction: {bin(inst._d)}, PC: {hex(PyRiscvOperator(32).unsigned(self._pc))}"
                )

                print("Write trace:")
                for k in range(0, 4):
                    print(f"Write trace for {k}th byte:")
                    for addr, r_inst, data in MEM_TRACER.get_memory_writes(
                        self._pc + k
                    ):
                        print(
                            f"{hex(PyRiscvOperator(32).unsigned(addr))}: {bin(r_inst[31:0])} -> {hex(data)}"
                        )
                    print()

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

            else:
                raise Exception("Invalid ecall number", self._regs[17])

            self._pc += 4

        else:
            raise Exception("Invalid instruction")


if __name__ == "__main__":
    import sys

    dmem = PyMEM(sys.argv[1])
    input_buffer = sys.argv[2]

    emulator = PyRiscv(dmem, reset_vec=0x00000000, input_buffer=input_buffer)
    instance = emulator
    emulator.run()
