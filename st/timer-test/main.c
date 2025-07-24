#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "plic.h"
#include "mtrap.h"
#include "ppu.h"
#include "csr.h"
#include "platform.h"
#include "dw_apb_timer.h"
#include <stdint.h>

#define TIMER_INTR_SOURCE_BASE 243

volatile uint8_t intr_cnt = 0;

void timer_intr_handler() {
  uint32_t intr = READ_U32(CTX_COMP_REG(0));
  timer_irq_handler(intr - TIMER_INTR_SOURCE_BASE - 1);
  WRITE_U32(CTX_COMP_REG(0), intr);
  intr_cnt++;

  atomic_printf("Core get external interrupt %d!\n", intr);
}

void enable_external_intr() {
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MEIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
}

int setup_plic() {
  plic_init(1);
  if(setup_context(0, 3)) return 1;
  for(int i = TIMER_INTR_SOURCE_BASE; i <= NR_INTR; i++) {
    if(setup_intr(i, 7)) return 1;
    if(enable_intr(0, i)) return 1;
  }
  return 0;
}

int main() {

  if(setup_plic()) return 1;
  printf("PLIC is initialized!\n");
  if(m_trap_handler_register(MEIP, timer_intr_handler)) return 1;

  for(int i = 0; i < 6; i++) {
    timer_init(i);
  }

  enable_external_intr();

  do{
    riscv_wfi();
  } while(intr_cnt < 6);
  
  printf("Timer test passed!\n");
}