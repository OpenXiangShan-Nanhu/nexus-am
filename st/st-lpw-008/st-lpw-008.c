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

void task0() {

  for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);

  while(step != 1);
  for(volatile int i = 0; i < 100; i++);
  
  switch_off_core(1);
  atomic_printf("Core 0 raise Core 1 ipi!\n");
  raise_ipi(1);
  switch_on_core(1);
}

void task1() {

  if(step != 0) { // run at second power-on
    return;
  }

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