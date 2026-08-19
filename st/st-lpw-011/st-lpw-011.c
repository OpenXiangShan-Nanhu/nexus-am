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
volatile int *reg = (int *)0x90000000;

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
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MEIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
  return 0;
}

void task0() {

  for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
  switch_power_mode(1, true, PWR_RET);

  step++;  // 1
  riscv_fence();


  for(volatile int i = 0; i < 2000; i++);
  int sum = 0;
  for(int i = 0; i < 100; i++){
    sum += READ_U64(reg + i * 0x40);
  }
  if(sum == 4950){
    atomic_printf("Core 2 get correct value!\n");
  }
  raise_ipi(1);

  while(step != 2);
  raise_ipi(1);
}

void task1() {
  ipi_init();
  if(m_trap_handler_register(MEIP, ipi_handler)) return;
  for(int i = 0; i < 100; i++){
    WRITE_U64(reg + i * 0x40, i);
  }
  riscv_fence();

  while(step != 1);
  riscv_wfi();
  atomic_printf("Core 1 wakeup by snoop!\n");
  step++;  // 2
  riscv_cbo_flush((uint64_t)&step);
  riscv_wfi();
  atomic_printf("Core 1 wakeup by intrrupt!\n");
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, empty, empty};

int main() {
  uint64_t hartid = riscv_mhartid();
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}