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
  switch_ret_core(1);
  step++; // 1
  riscv_fence();
}

void task1() {
  while(step != 1);
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, empty, empty};

int main() {
  uint64_t hartid = riscv_mhartid();
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}