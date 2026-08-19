#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include "cmo.h"
#include "ppu.h"
#include "clint.h"
#include "mtrap.h"
#include "csr.h"
#include "platform.h"

#define NUM_CORES 4

volatile int step = 0;

extern void atomic_add(uint64_t *addr, uint64_t val);

void ipi_handler() {
  if (imsic_ipi_claim() != IPI_EIID) {
    default_trap_handler();
    return;
  }
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %d get ipi!\n", id);
}

int ipi_init() {
  imsic_ipi_enable();
  csr_set(mie, MEIE);
  csr_set(mstatus, (0x1UL << 3));
  return 0;
}

void task0() {
  for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
  while(step != 3);

  for(volatile int i = 1; i < NUM_CORES; i++){
    atomic_printf("Core 0 try ipi to core %d\n", i);
    raise_ipi(i);
  }
}

void task1() {
  ipi_init();
  if(m_trap_handler_register(MEIP, ipi_handler)) return;

  atomic_add((uint64_t *)&step, 1);
  riscv_wfi();
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task1, task1};

int main() {

  uint64_t hartid = riscv_mhartid();
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}