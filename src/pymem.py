# PYMEM
# Author: jerry-jho@github

from collections import OrderedDict
from memory_tracer import MEM_TRACER
from pyriscv_operator import *

def PyMem_Iter(_mdata):
        _addr  = 0
        _max_addr = list(_mdata.keys())[-1]
        while True:
            if _addr > _max_addr:
                return
            else:
                v = _addr
                _addr += 1
                yield v

class PyMEM:
    FORMAT_VLOG_B8 = 1
    
    def __init__(self,file_or_fileobj,FORMAT=None):
        obj_close_flag = type(file_or_fileobj) == type("")
        if obj_close_flag:
            file_or_fileobj = open(file_or_fileobj,"r")
        self._mdata = OrderedDict()   
        if FORMAT is None:
            self.__read_vlog_b8(self._mdata, file_or_fileobj)
        if obj_close_flag:
            file_or_fileobj.close()
                
    def __read_vlog_b8(self,mdata,fileobj):
        addr = 0
        for line in fileobj:
            segs = line.strip().split(' ')
            for seg in segs:
                if seg == '':
                    continue
                if seg.startswith('@'):
                    addr = int(seg[1:],base=16)
                else:
                    data = int(seg,base=16)
                    mdata[addr] = data
                    addr += 1

    def __getitem__(self,addr):
        # k2 = addr
        # if k2 < 0:
        #     k2 += 1 << 32
        # k = self._mdata.get(addr, 0)
        # if k < 0:
        #     k += 1 << 32
        # print("loading data from memory", hex(PyRiscvOperator(32).unsigned(addr)), hex(self._mdata.get(PyRiscvOperator(32).unsigned(addr), 0)))
        # print("data in 2,147,483,696 to 2,147,483,699", self._mdata.get(2147483696, 114), self._mdata.get(2147483697, 0), self._mdata.get(2147483698, 0), self._mdata.get(2147483699, 0))

        return self._mdata.get(PyRiscvOperator(32).unsigned(addr), 0)
    
    def __setitem__(self,addr,data):
        # k = data
        # if k < 0:
        #     k += 1 << 32
        # k2 = addr
        # if k2 < 0:
        #     k2 += 1 << 32
        # print("writing data to memory", hex(PyRiscvOperator(32).unsigned(addr)), hex(data))

        MEM_TRACER.log_memory_write(addr, data)

        self._mdata[PyRiscvOperator(32).unsigned(addr)] = data
        
    def keys(self):
        return PyMem_Iter(self._mdata)
                    
if __name__ == '__main__':
    import sys
    o = PyMEM(sys.argv[1])
    print(o._mdata)
    for a in o.keys():
        print(a,o[a])
        
