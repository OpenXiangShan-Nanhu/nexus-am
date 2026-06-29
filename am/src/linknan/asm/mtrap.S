.section .text
.global _mtrap
.extern _c_mtrap
.type _mtrap, @function

.macro save_gpr reg_idx
  sd x\reg_idx, -\reg_idx*8(sp)
.endm

.macro save_fpr reg_idx
  fsd f\reg_idx, -\reg_idx*8(sp)
.endm

.macro restore_gpr reg_idx
  ld x\reg_idx, -\reg_idx*8(sp)
.endm

.macro restore_fpr reg_idx
  fld f\reg_idx, -\reg_idx*8(sp)
.endm

.macro save_regs
  .altmacro
  .set i, 0
  .rept 32
    save_gpr %i
    .set i, i + 1
  .endr
  addi sp, sp, -32 * 8
  .set i, 0
  .rept 32
    save_fpr %i
    .set i, i + 1
  .endr
  addi sp, sp, -32 * 8
  csrr t0, mepc
  sd t0, -0(sp)
  csrr t0, mstatus
  sd t0, -8(sp)
  addi sp, sp, -16
.endm

.macro restore_regs
  .altmacro
  addi sp, sp, 16
  ld t0, -0(sp)
  csrw mepc, t0
  ld t0, -8(sp)
  csrw mstatus, t0
  addi sp, sp, 32 * 8
  .set i, 0
  .rept 32
    restore_fpr %i
    .set i, i + 1
  .endr
  addi sp, sp, 32 * 8
  .set i, 0
  .rept 32
    restore_gpr %i
    .set i, i + 1
  .endr
.endm

.align 4
_mtrap:
  save_regs
  mv a0, sp
  call _c_mtrap
  restore_regs
  mret