from pymem import PyMEM
from pyriscv_types import PyRiscvLogic, PyRiscvStruct
import sys

dmem = PyMEM(sys.argv[1])
pc = 0x80000000

k = dmem[pc] + \
    (dmem[pc+1] << 8) + \
    (dmem[pc+2] << 16) + \
    (dmem[pc+3] << 24)

if k < 0:
    k += 1 << 32

inst = PyRiscvLogic(k)

w = inst

def num_to_binary_array(num, byte_len):
    ret = ""
    for _ in range(byte_len):
        ret = str(num & 0x1) + "b," + ret
        num >>= 1
    return "[" + ret[:-1] + "]"

decode_map = {}
decode_map["CODECLASS"]           = w[1:0]
decode_map["OPCODE"]              = w[6:2]
decode_map["FUNCT3_OP_IMM_OP"]    = w[14:12]
# decode_map["FUNCT3_BRANCH"]       = w[14:12]
# decode_map["FUNCT3_LOAD_STORE"]   = w[14:12]
decode_map["FUNCT7"]              = w[31:25]
decode_map["RD"]                  = w[11:7]
decode_map["RS1"]                 = w[19:15]
decode_map["RS2"]                 = w[24:20]
decode_map["IMMJ"]                = (w[30:21]<<1) | (w[20] << 11)   | (w[19:12] << 12) | (w[31] << 20)
decode_map["IMMB"]                = (w[11:8]<<1)  | (w[30:25] << 5) | (w[7] << 11)     | (w[31] << 12)
decode_map["IMMI"]                = w[31:20]
decode_map["IMMS"]                = w[11:7] + (w[31:25]<<5)
decode_map["IMMU"]                = w[31:12] << 12

decode_map_len = {}
decode_map_len["CODECLASS"]           = 2
decode_map_len["OPCODE"]              = 5
decode_map_len["FUNCT3_OP_IMM_OP"]    = 3
decode_map_len["FUNCT3_BRANCH"]       = 3
decode_map_len["FUNCT3_LOAD_STORE"]   = 3
decode_map_len["FUNCT7"]              = 7
decode_map_len["RD"]                  = 5
decode_map_len["RS1"]                 = 5
decode_map_len["RS2"]                 = 5
decode_map_len["IMMJ"]                = 21
decode_map_len["IMMB"]                = 13
decode_map_len["IMMI"]                = 12
decode_map_len["IMMS"]                = 12
decode_map_len["IMMU"]                = 32

# iter and print decode_map
for k,v in decode_map.items():
    print(k,":",num_to_binary_array(v,decode_map_len[k]))