#include <stdint.h>
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "csr.h"
#include "platform.h"

void default_strap_handler() {
  // uint32_t mhartid = csr_read(mhartid);
  uint64_t scause = csr_read(scause);
  int is_interrupt = (scause & (1UL << 63)) >> 63;
  uint64_t code = scause & (~(1UL << 63));
  if (is_interrupt) {
    switch (code) {
      case 0: s_atomic_printf( "S Mode: User software interrupt\n"); break;
      case 1: s_atomic_printf( "S Mode: Supervisor software interrupt\n"); break;
      case 4: s_atomic_printf( "S Mode: User timer interrupt\n"); break;
      case 5: s_atomic_printf( "S Mode: Supervisor timer interrupt\n"); break;
      case 8: s_atomic_printf( "S Mode: User external interrupt\n"); break;
      case 9: s_atomic_printf( "S Mode: Supervisor external interrupt\n"); break;
      default: s_atomic_printf("S Mode: Reserved interrupt code\n"); break;
    }
  } else {
    switch (code) {
      case 0: s_atomic_printf( "S mode: Instruction address misaligned\n"); break;
      case 1: s_atomic_printf( "S mode: Instruction access fault\n"); break;
      case 2: s_atomic_printf( "S mode: Illegal instruction\n"); break;
      case 3: s_atomic_printf( "S mode: Breakpoint\n"); break;
      case 4: s_atomic_printf( "S mode: Load address misaligned\n"); break;
      case 5: s_atomic_printf( "S mode: Load access fault\n"); break;
      case 6: s_atomic_printf( "S mode: Store/AMO address misaligned\n"); break;
      case 7: s_atomic_printf( "S mode: Store/AMO access fault\n"); break;
      case 8: s_atomic_printf( "S mode: Environment call from U-mode\n"); break;
      case 9: s_atomic_printf( "S mode: Environment call from S-mode\n"); break;
      case 12: s_atomic_printf("S mode: Instruction page fault\n"); break;
      case 13: s_atomic_printf("S mode: Load page fault\n"); break;
      case 15: s_atomic_printf("S mode: Store/AMO page fault\n"); break;
      default: s_atomic_printf("S mode: Reserved exception code\n"); break;
    }
  }
  uint64_t sepc = csr_read(sepc);
  uint64_t stval = csr_read(stval);
  s_atomic_printf("sepc: 0x%lx, stval: 0x%lx scause: 0x%lx\n", sepc, stval, scause);
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
    s_atomic_printf("Illegal scause code %lu\n", code);
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
    s_atomic_printf("Illegal intr code %lu, will not be register\n", code);
    return 1;
  } else if(is_interrupt) {
    s_atomic_printf("Registering interrupt %lu handler!\n", code);
    s_intr_handler_vector[code] = handler;
    riscv_fence();
    riscv_fence_i();
    return 0;
  } else {
    s_atomic_printf("Registering exception %lu handler!\n", code);
    s_ecpt_handler_vector[code] = handler;
    riscv_fence();
    riscv_fence_i();
    return 0;
  }
}