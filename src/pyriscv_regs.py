from pyriscv_operator import *

class PyRiscvRegs:
    def __init__(self,n,bw):
        self._regs = [0] * n
        self._bw = bw
        
    def __getitem__(self,a):        
        return self._regs[a]
    
    def __setitem__(self,a,v):
        if a == 0:
            return
        
        # print("X%d <= %d(%x)" % (a,v,PyRiscvOperator(32).unsigned(v)))
        
        self._regs[a] = v

        # fmt = '0%dx' % (self._bw / 4)
        # fmt = '%' + fmt
        # hv = fmt % v
        # if a == 0:
        #     print("[DEBUG] 0x%s(%d)" % (hv,v))
        # else:               
            
        #     #print("X%d <= %d(%x)" % (a,v,v))
        #     self._regs[a] = v

    def __str__(self):
        s = ""
        for i in range(len(self._regs)):
            k = self.__getitem__(i)
            if k < 0:
                k += 1 << 32
            
            s += "X%d: %x\n" % (i, k)
        return s

    def to_dict(self):
        s = {}
        for i in range(len(self._regs)):
            k = self.__getitem__(i)
            if k < 0:
                k += 1 << 32
            
            s[format(i, '05b')] = format(k, '0%dx' % (self._bw / 4))
        return s
    
    def to_dict_str(self):
        d = self.to_dict()
        s = "{"
        for k, v in d.items():
            s += "%s: \"%s\", " % (k, v)
        return s[:-2] + "}"