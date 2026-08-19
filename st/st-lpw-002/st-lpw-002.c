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
#define stuck(x) for(volatile int i = 0; i < x; i++)

volatile int step = 0;
volatile int *reg = (int *)0x90000000;

void ipi_handler() {
  if (imsic_ipi_claim() != IPI_EIID) {
    default_trap_handler();
    return;
  }
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

  while(step != 1);
  stuck(1000);
  switch_ret_core(1);
  step++;  // 2
  stuck(1000);
  switch_on_core(1);
  raise_ipi(1);
}

void task1() {
  ipi_init();
  if(m_trap_handler_register(MEIP, ipi_handler)) return;

  *reg = 0x12345678;  // wait for snoop
  step++; // 1
  riscv_cbo_flush((uint64_t)&step);
  riscv_wfi();
}

void task2(){
  while(step != 2);
  atomic_printf("Core 2 try to snoop core 1\n");
  step++;
  if(*reg == 0x12345678) atomic_printf("Core 2 get value!\n");
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task2, empty};

int main() {
  uint64_t hartid = riscv_mhartid();
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}