.global start

.macro START_CORE hartid
    la t0, 0x200000 + \hartid * 0x10000
    li t1, 2
    sw t1, 0(t0)
.endm

start:
    csrr a0, mhartid
    bnez a0, run
    START_CORE 1
    START_CORE 2
    START_CORE 3
run:
    li t0, 0x80000000
    jr t0