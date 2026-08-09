.section entry, "ax"
.global _start
.type _start, @function
.extern _mtrap
#define MSTATUS_FS 0x00006000
#define MNSTATUS_NMIE 0x8

.macro set_reg_zero reg_idx
  mv x\reg_idx, zero
.endm

.macro set_freg_zero freg_idx
  fmv.w.x f\freg_idx, zero
.endm

.macro init_regs
  .altmacro
  .set i, 1
  .rept 31
    set_reg_zero %i
    .set i, i+1
  .endr
.endm

.macro init_fregs
  .set i, 0
  .rept 32
    set_freg_zero %i
    .set i, i+1
  .endr
.endm

_start:
  init_regs

  li a0, MSTATUS_FS & (MSTATUS_FS >> 1)
  csrs mstatus, a0
  csrwi fcsr, 0
  la t0, _mtrap
  csrw mtvec, t0

  # Nanhu 5.2 resets mnstatus.NMIE to 0, which masks all regular interrupts.
  li t0, MNSTATUS_NMIE
  csrs 0x744, t0

  init_fregs # init fregs after fp enable

  la t0, _stack_top
  la t1, _stack_pointer
  sub t3, t1, t0
  csrr t4, mhartid
  mul t5, t3, t4
  add sp, t5, t1
#   bnez t4, _skip_bss

#   la t0, _bss_start
#   la t1, _stack_top
#   beq t0, t1, _skip_bss

# _bss_loop:
#   sd zero, (t0)
#   addi t0, t0, 8
#   blt t0, t1, _bss_loop

# _skip_bss:
  jal _trm_init
