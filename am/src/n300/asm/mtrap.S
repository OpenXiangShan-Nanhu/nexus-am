.section .text
.global _mtrap
.extern _c_mtrap
.type _mtrap, @function

.macro save_gpr reg_idx
  sw x\reg_idx, -\reg_idx*4(sp)
.endm

.macro save_fpr reg_idx
  fsw f\reg_idx, -\reg_idx*4(sp)
.endm

.macro restore_gpr reg_idx
  lw x\reg_idx, -\reg_idx*4(sp)
.endm

.macro restore_fpr reg_idx
  flw f\reg_idx, -\reg_idx*4(sp)
.endm

.macro save_regs
  .altmacro
  .set i, 0
  .rept 32
    save_gpr %i
    .set i, i + 1
  .endr
  addi sp, sp, -32 * 4
  .set i, 0
  .rept 32
    save_fpr %i
    .set i, i + 1
  .endr
  addi sp, sp, -32 * 4
  csrr t0, mepc
  sw t0, -0(sp)
  csrr t0, mstatus
  sw t0, -4(sp)
  addi sp, sp, -8
.endm

.macro restore_regs
  .altmacro
  addi sp, sp, 8
  lw t0, -0(sp)
  csrw mepc, t0
  lw t0, -4(sp)
  csrw mstatus, t0
  addi sp, sp, 32 * 4
  .set i, 0
  .rept 32
    restore_fpr %i
    .set i, i + 1
  .endr
  addi sp, sp, 32 * 4
  .set i, 0
  .rept 32
    restore_gpr %i
    .set i, i + 1
  .endr
.endm

.align 4
_mtrap:
  save_regs
  call _c_mtrap
  restore_regs
  mret
