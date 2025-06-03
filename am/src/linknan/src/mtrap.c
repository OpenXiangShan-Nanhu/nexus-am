#include <stdint.h>
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "csr.h"
#include "platform.h"

void default_trap_handler() {
  printf("Default trap handler is called!\n");
  uint64_t mcause = csr_read(mcause);
  int is_interrupt = (mcause & (1UL << 63)) >> 63;
  uint64_t code = mcause & (~(1UL << 63));
  if (is_interrupt) {
    printf("Interrupt detected. Code: %lu\n", code);
    switch (code) {
      case 0: printf("User software interrupt\n"); break;
      case 1: printf("Supervisor software interrupt\n"); break;
      case 3: printf("Machine software interrupt\n"); break;
      case 4: printf("User timer interrupt\n"); break;
      case 5: printf("Supervisor timer interrupt\n"); break;
      case 7: printf("Machine timer interrupt (MTIP is asserted!)\n"); break;
      case 8: printf("User external interrupt\n"); break;
      case 9: printf("Supervisor external interrupt\n"); break;
      case 11: printf("Machine external interrupt (MEIP is asserted!)\n"); break;
      default: printf("Reserved interrupt code\n"); break;
    }
  } else {
    printf("Exception detected. Code: %lu\n", code);
    switch (code) {
      case 0: printf("Instruction address misaligned\n"); break;
      case 1: printf("Instruction access fault\n"); break;
      case 2: printf("Illegal instruction\n"); break;
      case 3: printf("Breakpoint\n"); break;
      case 4: printf("Load address misaligned\n"); break;
      case 5: printf("Load access fault\n"); break;
      case 6: printf("Store/AMO address misaligned\n"); break;
      case 7: printf("Store/AMO access fault\n"); break;
      case 8: printf("Environment call from U-mode\n"); break;
      case 9: printf("Environment call from S-mode\n"); break;
      case 11: printf("Environment call from M-mode\n"); break;
      case 12: printf("Instruction page fault\n"); break;
      case 13: printf("Load page fault\n"); break;
      case 15: printf("Store/AMO page fault\n"); break;
      default: printf("Reserved exception code\n"); break;
    }
  }
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
    printf("Illegal mcause code %lu\n", code);
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
    printf("Illegal intr code %lu, will not be register\n", code);
    return 1;
  } else if(is_interrupt) {
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