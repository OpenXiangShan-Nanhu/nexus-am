#include <stdint.h>
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "csr.h"
#include "platform.h"

void default_strap_handler() {
  uint32_t mhartid = csr_read(mhartid);
  uint64_t scause = csr_read(scause);
  int is_interrupt = (scause & (1UL << 63)) >> 63;
  uint64_t code = scause & (~(1UL << 63));
  if (is_interrupt) {
    switch (code) {
      case 0: atomic_printf("Core %d User software interrupt\n", mhartid); break;
      case 1: atomic_printf("Core %d Supervisor software interrupt\n", mhartid); break;
      case 4: atomic_printf("Core %d User timer interrupt\n", mhartid); break;
      case 5: atomic_printf("Core %d Supervisor timer interrupt\n", mhartid); break;
      case 8: atomic_printf("Core %d User external interrupt\n", mhartid); break;
      case 9: atomic_printf("Core %d Supervisor external interrupt\n", mhartid); break;
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
      case 12: atomic_printf("Core %d Instruction page fault\n", mhartid); break;
      case 13: atomic_printf("Core %d Load page fault\n", mhartid); break;
      case 15: atomic_printf("Core %d Store/AMO page fault\n", mhartid); break;
      default: atomic_printf("Core %d Reserved exception code\n", mhartid); break;
    }
  }
  uint64_t sepc = csr_read(sepc);
  uint64_t stval = csr_read(stval);
  atomic_printf("sepc: 0x%lx, stval: 0x%lx scause: 0x%lx\n", sepc, stval, scause);
}

void (*s_intr_handler_vector[16])(void) = {
  default_strap_handler, default_strap_handler, default_strap_handler, default_strap_handler,
  default_strap_handler, default_strap_handler, default_strap_handler, default_strap_handler,
  default_strap_handler, default_strap_handler, default_strap_handler, default_strap_handler,
  default_strap_handler, default_strap_handler, default_strap_handler, default_strap_handler
};

void (*s_ecpt_handler_vector[16])(void) = {
  default_strap_handler, default_strap_handler, default_strap_handler, default_strap_handler,
  default_strap_handler, default_strap_handler, default_strap_handler, default_strap_handler,
  default_strap_handler, default_strap_handler, default_strap_handler, default_strap_handler,
  default_strap_handler, default_strap_handler, default_strap_handler, default_strap_handler
};

void _c_strap() {
  uint64_t scause = csr_read(scause);
  int is_interrupt = (scause & (1UL << 63)) >> 63;
  uint64_t code = scause & (~(1UL << 63));
  if(code > 16) {
    atomic_printf("Illegal scause code %lu\n", code);
    return;
  }
  if(is_interrupt) {
    s_intr_handler_vector[code]();
  } else {
    s_ecpt_handler_vector[code]();
  }
}

int s_trap_handler_register(uint64_t cause, void handler(void)) {
  int is_interrupt = (cause & (1UL << 63)) >> 63;
  uint64_t code = cause & (~(1UL << 63));
  if(code > 16) {
    atomic_printf("Illegal intr code %lu, will not be register\n", code);
    return 1;
  } else if(is_interrupt) {
    atomic_printf("Registering interrupt %lu handler!\n", code);
    s_intr_handler_vector[code] = handler;
    riscv_fence();
    riscv_fence_i();
    return 0;
  } else {
    atomic_printf("Registering exception %lu handler!\n", code);
    s_ecpt_handler_vector[code] = handler;
    riscv_fence();
    riscv_fence_i();
    return 0;
  }
}