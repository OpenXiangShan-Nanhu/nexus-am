#include <stdint.h>
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <xsextra.h>
#include "csr.h"
#include "riscv.h"
#include "platform.h"

/*
 * Misaligned load/store software emulation.
 *
 * When the hardware raises cause 4 (load misaligned) or 6 (store misaligned),
 * _c_mtrap receives the saved register frame via a0 (sp at trap entry).
 * We decode the faulting instruction, perform byte-by-byte access,
 * write the result to the target register in the frame, advance mepc, and return.
 *
 * Frame layout (set by save_regs in mtrap.S):
 *   save_gpr: sd x_i, -i*8(sp) for i=0..31, then sp -= 256
 *   save_fpr: fsd f_i, -i*8(sp) for i=0..31, then sp -= 256
 *   sd mepc, 0(sp); sd mstatus, -8(sp); sp -= 16
 *
 * At call site (sp passed as a0):
 *   sp+16  = mepc
 *   sp+8   = mstatus
 *   sp+16+256 = base of FPR area (f0 at offset +16+256, f31 at +16+256-31*8)
 *   sp+16+256+256 = base of GPR area (x0 at that addr, x31 at -31*8 below it)
 *
 * GPR x[i] is at: frame + 16 + 256 + 256 - i*8 = frame + 528 - i*8
 * mepc is at: frame + 16
 */

#define FRAME_MEPC_OFF    16
#define FRAME_GPR_BASE    528  /* x0 at frame+528, x1 at frame+520, ..., x31 at frame+280 */

static inline uint64_t *frame_gpr(uint64_t *frame, int reg) {
  return (uint64_t *)((char *)frame + FRAME_GPR_BASE - reg * 8);
}

static inline uint64_t *frame_mepc(uint64_t *frame) {
  return (uint64_t *)((char *)frame + FRAME_MEPC_OFF);
}

static inline int insn_rd(uint32_t insn)  { return (insn >> 7)  & 0x1f; }
static inline int insn_rs2(uint32_t insn) { return (insn >> 20) & 0x1f; }

static uint64_t load_bytes(uint64_t addr, int n) {
  uint64_t val = 0;
  for (int i = 0; i < n; i++)
    val |= (uint64_t)(*(volatile uint8_t *)(addr + i)) << (8 * i);
  return val;
}

static int64_t load_bytes_signed(uint64_t addr, int n) {
  uint64_t val = load_bytes(addr, n);
  int shift = 64 - n * 8;
  return (int64_t)(val << shift) >> shift;
}

static void store_bytes(uint64_t addr, uint64_t val, int n) {
  for (int i = 0; i < n; i++)
    *(volatile uint8_t *)(addr + i) = (val >> (8 * i)) & 0xff;
}

/*
 * Emulate misaligned load/store. Returns 0 on success, -1 if unrecognized.
 * Handles RV64I: LH LHU LW LWU LD SH SW SD
 * Also handles RV64C compressed loads/stores: C.LW C.LD C.SW C.SD C.LWSP C.LDSP C.SWSP C.SDSP
 */
static int emulate_misaligned(uint64_t *frame, int is_store) {
  uint64_t addr = csr_read(mtval);
  uint64_t pc = *frame_mepc(frame);
  uint32_t insn = *(uint16_t *)pc;  /* read first 16 bits */
  int insn_len;

  if ((insn & 0x3) == 0x3) {
    /* 32-bit instruction */
    insn = *(uint32_t *)pc;
    insn_len = 4;

    if (is_store) {
      int rs2 = insn_rs2(insn);
      uint64_t val = *frame_gpr(frame, rs2);
      uint32_t funct3 = (insn >> 12) & 0x7;
      switch (funct3) {
        case 0x1: store_bytes(addr, val, 2); break;  /* SH */
        case 0x2: store_bytes(addr, val, 4); break;  /* SW */
        case 0x3: store_bytes(addr, val, 8); break;  /* SD */
        default: return -1;
      }
    } else {
      int rd = insn_rd(insn);
      uint32_t funct3 = (insn >> 12) & 0x7;
      uint64_t val;
      switch (funct3) {
        case 0x1: val = (uint64_t)load_bytes_signed(addr, 2); break;  /* LH  */
        case 0x2: val = (uint64_t)load_bytes_signed(addr, 4); break;  /* LW  */
        case 0x3: val =           load_bytes       (addr, 8); break;  /* LD  */
        case 0x5: val =           load_bytes       (addr, 2); break;  /* LHU */
        case 0x6: val =           load_bytes       (addr, 4); break;  /* LWU */
        default: return -1;
      }
      if (rd != 0) *frame_gpr(frame, rd) = val;
    }
  } else {
    /* 16-bit compressed instruction */
    insn_len = 2;
    uint16_t ci = (uint16_t)insn;
    uint8_t op = ci & 0x3;
    uint8_t funct3 = (ci >> 13) & 0x7;

    if (op == 0x0) {
      /* CL/CS format: C.LW(010) C.LD(011) C.SW(110) C.SD(111) */
      int rd_rs2 = ((ci >> 2) & 0x7) + 8;  /* r' -> x8..x15 */
      if (is_store) {
        uint64_t val = *frame_gpr(frame, rd_rs2);
        if (funct3 == 0x6) store_bytes(addr, val, 4);       /* C.SW */
        else if (funct3 == 0x7) store_bytes(addr, val, 8);  /* C.SD */
        else return -1;
      } else {
        uint64_t val;
        if (funct3 == 0x2) val = (uint64_t)load_bytes_signed(addr, 4);      /* C.LW */
        else if (funct3 == 0x3) val = load_bytes(addr, 8);                   /* C.LD */
        else return -1;
        *frame_gpr(frame, rd_rs2) = val;
      }
    } else if (op == 0x2) {
      /* CI/CSS format: C.LWSP(010) C.LDSP(011) C.SWSP(110) C.SDSP(111) */
      if (is_store) {
        int rs2 = (ci >> 2) & 0x1f;
        uint64_t val = *frame_gpr(frame, rs2);
        if (funct3 == 0x6) store_bytes(addr, val, 4);       /* C.SWSP */
        else if (funct3 == 0x7) store_bytes(addr, val, 8);  /* C.SDSP */
        else return -1;
      } else {
        int rd = (ci >> 7) & 0x1f;
        uint64_t val;
        if (funct3 == 0x2) val = (uint64_t)load_bytes_signed(addr, 4);      /* C.LWSP */
        else if (funct3 == 0x3) val = load_bytes(addr, 8);                   /* C.LDSP */
        else return -1;
        if (rd != 0) *frame_gpr(frame, rd) = val;
      }
    } else {
      return -1;
    }
  }

  *frame_mepc(frame) = pc + insn_len;
  return 0;
}

/* ---- default handler (unchanged signature) ---- */

void default_trap_handler() {
  uint32_t mhartid = csr_read(mhartid);
  uint64_t mcause = csr_read(mcause);
  int is_interrupt = (mcause >> 63) & 1;
  uint64_t code = mcause & ~(1UL << 63);
  if (is_interrupt) {
    switch (code) {
      case 0: printf("Core %d User software interrupt\n", mhartid); break;
      case 1: printf("Core %d Supervisor software interrupt\n", mhartid); break;
      case 3: printf("Core %d Machine software interrupt\n", mhartid); break;
      case 4: printf("Core %d User timer interrupt\n", mhartid); break;
      case 5: printf("Core %d Supervisor timer interrupt\n", mhartid); break;
      case 7: printf("Core %d Machine timer interrupt (MTIP is asserted!)\n", mhartid); break;
      case 8: printf("Core %d User external interrupt\n", mhartid); break;
      case 9: printf("Core %d Supervisor external interrupt\n", mhartid); break;
      case 11: printf("Core %d Machine external interrupt (MEIP is asserted!)\n", mhartid); break;
      default: printf("Core %d Reserved interrupt code\n", mhartid); break;
    }
  } else {
    switch (code) {
      case 0: printf("Core %d Instruction address misaligned\n", mhartid); break;
      case 1: printf("Core %d Instruction access fault\n", mhartid); break;
      case 2: printf("Core %d Illegal instruction\n", mhartid); break;
      case 3: printf("Core %d Breakpoint\n", mhartid); break;
      case 4: printf("Core %d Load address misaligned\n", mhartid); break;
      case 5: printf("Core %d Load access fault\n", mhartid); break;
      case 6: printf("Core %d Store/AMO address misaligned\n", mhartid); break;
      case 7: printf("Core %d Store/AMO access fault\n", mhartid); break;
      case 8: printf("Core %d Environment call from U-mode\n", mhartid); break;
      case 9: printf("Core %d Environment call from S-mode\n", mhartid); break;
      case 11: printf("Core %d Environment call from M-mode\n", mhartid); break;
      case 12: printf("Core %d Instruction page fault\n", mhartid); break;
      case 13: printf("Core %d Load page fault\n", mhartid); break;
      case 15: printf("Core %d Store/AMO page fault\n", mhartid); break;
      default: printf("Core %d Reserved exception code\n", mhartid); break;
    }
  }
  uint64_t mepc = csr_read(mepc);
  uint64_t mtval = csr_read(mtval);
  printf("mepc: 0x%lx, mtval: 0x%lx mcause: 0x%lx\n", mepc, mtval, mcause);
}

void (*intr_handler_vector[16])(void) = {
  default_trap_handler, default_trap_handler, default_trap_handler, default_trap_handler,
  default_trap_handler, default_trap_handler, default_trap_handler, default_trap_handler,
  default_trap_handler, default_trap_handler, default_trap_handler, default_trap_handler,
  default_trap_handler, default_trap_handler, default_trap_handler, default_trap_handler
};

void (*ecpt_handler_vector[16])(void) = {
  default_trap_handler, default_trap_handler, default_trap_handler, default_trap_handler,
  default_trap_handler, default_trap_handler, default_trap_handler, default_trap_handler,
  default_trap_handler, default_trap_handler, default_trap_handler, default_trap_handler,
  default_trap_handler, default_trap_handler, default_trap_handler, default_trap_handler
};

/*
 * _c_mtrap: called from mtrap.S with a0 = sp (register frame pointer).
 * For misaligned load/store (cause 4/6), we emulate in software and return.
 * For all other traps, dispatch to the handler vector as before.
 */
void _c_mtrap(uint64_t *frame) {
  uint64_t mcause = csr_read(mcause);
  int is_interrupt = (mcause >> 63) & 1;
  uint64_t code = mcause & ~(1UL << 63);

  if (!is_interrupt && (code == 4 || code == 6)) {
    if (emulate_misaligned(frame, (code == 6)) != 0) {
      printf("Failed to emulate misaligned %s @ mepc=0x%lx addr=0x%lx\n",
             code == 4 ? "load" : "store",
             *frame_mepc(frame), csr_read(mtval));
      _halt(1);
    }
    return;
  }

  if (code >= 16) {
    printf("Illegal mcause code %lu\n", code);
    return;
  }
  if (is_interrupt)
    intr_handler_vector[code]();
  else
    ecpt_handler_vector[code]();
}

int m_trap_handler_register(uint64_t cause, void handler(void)) {
  int is_interrupt = (cause & (1UL << 63)) >> 63;
  uint64_t code = cause & (~(1UL << 63));
  if (code > 16) {
    printf("Illegal intr code %lu, will not be register\n", code);
    return 1;
  } else if (is_interrupt) {
    printf("Registering interrupt %lu handler!\n", code);
    intr_handler_vector[code] = handler;
    riscv_fence();
    riscv_fence_i();
    return 0;
  } else {
    printf("Registering exception %lu handler!\n", code);
    ecpt_handler_vector[code] = handler;
    riscv_fence();
    riscv_fence_i();
    return 0;
  }
}

extern char _strap;

void switch_mode(uint64_t hartid, uint64_t next_mode, uint64_t next_pc) {

  s_atomic_printf("Core %d switch to mode %d\n", hartid, next_mode);

  csr_set(mstatus, MSTATUS_SPP(MODE_S));
  csr_write(sepc, next_pc);

  csr_write(stvec, &_strap);
  csr_write(sscratch, 0);

  init_pmp();
  asm volatile(
    "mv a0, %0\n"
    "sret;"
    : : "r"(hartid) : "memory");

}

void m_switch_mode(uint64_t hartid, uint64_t next_mode, uint64_t next_pc) {

  printf("Core %d switch to mode %d\n", hartid, next_mode);

  uint64_t val = csr_read(mstatus);;
  val = val & (~MSTATUS_MPP(MODE_M));
  val = val | MSTATUS_MPP(next_mode);
  csr_write(mstatus, val);
  csr_write(mepc, next_pc);

  csr_write(stvec, &_strap);
  csr_write(sscratch, 0);
  csr_write(sie, 0);

  init_pmp();
  asm volatile(
    "mv a0, %0\n"
    "mret;"
    : : "r"(hartid) : "memory");

}
