    .global start
start:
    csrr a0, mhartid
    bnez a0, end

power_on_cores:
    la a1, 0x101000
    la a2, 0x201000
    la a3, 0x301000
    li a4, 2

    sw a4, 0(a1)
    sw a4, 0(a2)
    sw a4, 0(a3)

end:
    addiw   t0, zero,1
    slli    t0, t0, 0x1f
    jr      t0

