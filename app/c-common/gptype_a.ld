INCLUDE ../c-common/link_basic.ld

__global_pointer$ = MIN(__PREINIT_ARRAY_BEGIN__ + 0x800,
                        MAX(__DATA_BEGIN__ + 0x800, __BSS_END__ - 0x800));