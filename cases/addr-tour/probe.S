// Minimal M-mode trap entry for the addr-tour probes.
// For LOAD/STORE access faults it records mtval and skips the faulting
// 32-bit instruction (mepc += 4); everything else falls through to the AM
// trap entry (_mtrap).  Only t0-t2 are used and they are saved/restored.
    .section .text
    .globl probe_mtrap
    .type probe_mtrap, @function
    .align 2
probe_mtrap:
    addi sp, sp, -24
    sd   t0, 0(sp)
    sd   t1, 8(sp)
    sd   t2, 16(sp)

    csrr t0, mcause
    srli t1, t0, 63            // interrupt bit
    bnez t1, 1f                // interrupt -> AM trap
    li   t1, 5                 // LOAD_ACC_FAULT
    beq  t0, t1, 2f
    li   t1, 7                 // STORE_ACC_FAULT
    beq  t0, t1, 2f
1:
    ld   t0, 0(sp)
    ld   t1, 8(sp)
    ld   t2, 16(sp)
    addi sp, sp, 24
    j    _mtrap

2:
    csrr t1, mtval
    la   t2, fault_addr
    sd   t1, 0(t2)
    li   t1, 1
    la   t2, fault_flag
    sw   t1, 0(t2)
    la   t2, fault_count
    ld   t1, 0(t2)
    addi t1, t1, 1
    sd   t1, 0(t2)
    csrr t0, mepc
    addi t0, t0, 4             // skip the faulting 32-bit ld/sd
    csrw mepc, t0
    ld   t0, 0(sp)
    ld   t1, 8(sp)
    ld   t2, 16(sp)
    addi sp, sp, 24
    mret
