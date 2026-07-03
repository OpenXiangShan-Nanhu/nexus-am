# machine-mode timer interrupt handler from xv6 code

.globl __am_timervec
.align 4
__am_timervec:
        csrrw a0, mscratch, a0
        sw a1, 16(a0)
        sw a2, 20(a0)
        sw a3, 24(a0)
        sw a4, 28(a0)
        sw a5, 32(a0)

        # read mcause
        csrr a1, mcause
        # test whether it's an illegal instruction
        li a2, 2
        beq a1, a2, is_illegal
        # test whether it's a timer interrupt
        li a2, 1
        slli a2, a2, 31
        ori a2, a2, 7
        bne a1, a2, is_ext

is_timer:
        # schedule the next timer interrupt
        # by adding interval to mtimecmp.
        lw a1, 0(a0)   # CLINT_MTIMECMP(hart) address
        lw a2, 4(a0)   # interval low
        lw a3, 8(a0)   # interval high
        lw a4, 0(a1)   # mtimecmp low
        lw a5, 4(a1)   # mtimecmp high
        add a4, a4, a2
        sltu a2, a4, a2
        add a5, a5, a3
        add a5, a5, a2
        sw a4, 0(a1)
        sw a5, 4(a1)

        # raise a supervisor timer interrupt.
        li a3, 32
        csrw mip, a3
        j end_of_intr

is_ext:
        # disable machine mode external interrupt (mie.meie)
        li a3, 0x800
        csrc mie, a3
        # raise a supervisor external interrupt
        li a3, 0x200
        csrs mip, a3

        j end_of_intr

is_illegal:
        # for now, we assume that when there's an illegal instruction exception,
        # the supervisor tries to write mie (enable mie.meie).
        li a3, 0x800
        csrs mie, a3
        # clear the supervisor external and timer interrupt
        li a3, 0x220
        csrc mip, a3
        csrr a3, mepc
        addi a3, a3, 4
        csrw mepc, a3

end_of_intr:
        lw a5, 32(a0)
        lw a4, 28(a0)
        lw a3, 24(a0)
        lw a2, 20(a0)
        lw a1, 16(a0)
        csrrw a0, mscratch, a0

        mret
