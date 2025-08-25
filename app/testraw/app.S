.section .text
.globl _start

_start:
    # write(STDOUT, "MCBBS", 5)
    li a0, 1                  # fd: stdout (1)
    la a1, msg                # buffer: address of "MCBBS"
    li a2, 6                  # count: length of string
    li a7, 64                 # syscall number for write
    ecall                     # make system call

    # exit(0)
    li a0, 0                  # exit status
    li a7, 93                 # syscall number for exit
    ecall

    # Data section
    .section .rodata
msg:
    .ascii "MCBBS\n"