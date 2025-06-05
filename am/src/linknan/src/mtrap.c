#include <stdint.h>
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "csr.h"
#include "platform.h"

void default_trap_handler() {
  uint32_t mhartid = csr_read(mhartid);
  uint64_t mcause = csr_read(mcause);
  int is_interrupt = (mcause & (1UL << 63)) >> 63;
  uint64_t code = mcause & (~(1UL << 63));
  if (is_interrupt) {
    switch (code) {
      case 0: atomic_printf("Core %d User software interrupt\n", mhartid); break;
      case 1: atomic_printf("Core %d Supervisor software interrupt\n", mhartid); break;
      case 3: atomic_printf("Core %d Machine software interrupt\n", mhartid); break;
      case 4: atomic_printf("Core %d User timer interrupt\n", mhartid); break;
      case 5: atomic_printf("Core %d Supervisor timer interrupt\n", mhartid); break;
      case 7: atomic_printf("Core %d Machine timer interrupt (MTIP is asserted!)\n", mhartid); break;
      case 8: atomic_printf("Core %d User external interrupt\n", mhartid); break;
      case 9: atomic_printf("Core %d Supervisor external interrupt\n", mhartid); break;
      case 11: atomic_printf("Core %d Machine external interrupt (MEIP is asserted!)\n", mhartid); break;
      default: atomic_printf("Core %d Reserved interrupt code\n", mhartid); break;
    }
  } else {
    switch (code) {
      case 0: atomic_printf("Core %d Instruction address misaligned\n", mhartid); break;
      case 1: atomic_printf("Core %d Instruction access fault\n", mhartid); break;
      case 2: atomic_printf("Core %d Illegal instruction\n", mhartid); break;
      case 3: atomic_printf("Core %d Breakpoint\n", mhartid); break;
      case 4: atomic_printf("Core %d Load address misaligned\n", mhartid); break;
      case 5: atomic_printf("Core %d Load access fault\n", mhartid); break;
      case 6: atomic_printf("Core %d Store/AMO address misaligned\n", mhartid); break;
      case 7: atomic_printf("Core %d Store/AMO access fault\n", mhartid); break;
      case 8: atomic_printf("Core %d Environment call from U-mode\n", mhartid); break;
      case 9: atomic_printf("Core %d Environment call from S-mode\n", mhartid); break;
      case 11: atomic_printf("Core %d Environment call from M-mode\n", mhartid); break;
      case 12: atomic_printf("Core %d Instruction page fault\n", mhartid); break;
      case 13: atomic_printf("Core %d Load page fault\n", mhartid); break;
      case 15: atomic_printf("Core %d Store/AMO page fault\n", mhartid); break;
      default: atomic_printf("Core %d Reserved exception code\n", mhartid); break;
    }
  }
  uint64_t mepc = csr_read(mepc);
  uint64_t mtval = csr_read(mtval);
  atomic_printf("mepc: 0x%lx, mtval: 0x%lx mcause: 0x%lx\n", mepc, mtval, mcause);
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

void _c_mtrap() {
  uint64_t mcause = csr_read(mcause);
  int is_interrupt = (mcause & (1UL << 63)) >> 63;
  uint64_t code = mcause & (~(1UL << 63));
  if(code > 16) {
    atomic_printf("Illegal mcause code %lu\n", code);
    return;
  }
  if(is_interrupt) {
    intr_handler_vector[code]();
  } else {
    ecpt_handler_vector[code]();
  }
}

int m_trap_handler_register(uint64_t cause, void handler(void)) {
  int is_interrupt = (cause & (1UL << 63)) >> 63;
  uint64_t code = cause & (~(1UL << 63));
  if(code > 16) {
    atomic_printf("Illegal intr code %lu, will not be register\n", code);
    return 1;
  } else if(is_interrupt) {
    atomic_printf("Registering interrupt %lu handler!\n", code);
    intr_handler_vector[code] = handler;
    riscv_fence();
    riscv_fence_i();
    return 0;
  } else {
    atomic_printf("Registering exception %lu handler!\n", code);
    ecpt_handler_vector[code] = handler;
    riscv_fence();
    riscv_fence_i();
    return 0;
  }
}