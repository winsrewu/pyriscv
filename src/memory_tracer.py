class MemoryTracer:
    def __init__(self):
        self.memory = dict()
        self.pc = 0
        self.r_inst = 0

    def set_running_inst(self, pc, r_inst):
        self.pc = pc
        self.r_inst = r_inst

    def log_memory_write(self, addr, data):
        if addr not in self.memory:
            self.memory[addr] = []
        self.memory[addr].append((self.pc, self.r_inst, data))

    def get_memory_writes(self, addr):
        return self.memory.get(addr, [])


MEM_TRACER = MemoryTracer()
