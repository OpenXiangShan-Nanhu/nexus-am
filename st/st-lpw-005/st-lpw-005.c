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
volatile int flag = 0;

__attribute__((noinline)) void dyn_code(){
  flag = 1;
  return; 
}

__attribute__((noinline)) void tgt_code(){
  flag = 2;
  return;
}


void ipi_handler() {
  uint64_t id = riscv_mhartid();
  atomic_printf("Core 1 fence_i\n");
  riscv_fence_i();

  dyn_code(); // the function shoule run tgt_code
  if(flag == 1)
    atomic_printf("Should not arrive here!\n");

  if(flag == 2)
    atomic_printf("Core 1 has run the modified code\n");

  clear_ipi(id);
}

int ipi_init() {
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MSIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
  return 0;
}

void task0() {
  for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);

  while(step != 1);
  for(volatile int i = 0; i < 100; i++);

  switch_ret_core(1);

  atomic_printf("Core 0 modify code\n");
  uint8_t *src = (uint8_t *)tgt_code;
  uint8_t *dst = (uint8_t *)dyn_code;
  memcpy(dst, src, 4);

  riscv_fence();
  riscv_fence_i();

  raise_ipi(1);
  switch_on_core(1);
}

void task1(uint64_t id) {
  ipi_init();
  if(m_trap_handler_register(MSIP, ipi_handler)) return;

  step++; // 1
  riscv_cbo_flush((uint64_t)&step);
  
  riscv_wfi();
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, empty, empty};

int main() {
  uint64_t hartid = riscv_mhartid();
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}