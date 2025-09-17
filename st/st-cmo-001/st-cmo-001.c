#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include <stdint.h>
#include "cmo.h"
#include "ppu.h"
#include "clint.h"
#include "mtrap.h"
#include "csr.h"
#include "platform.h"

#define NUM_CORES 4

volatile uint64_t step = 0;
volatile void *pmem = (void *)0x90000000;

extern uint64_t atomic_swap(volatile uint64_t *, uint64_t);

void task0() {
  uint64_t hartid = riscv_mhartid();
  atomic_swap((void *)pmem, 0xdeedbeef);
  atomic_swap((void *)&step, 1);
  barrier(NUM_CORES);
  riscv_cbo_clean((uint64_t)pmem);
  uint64_t val = READ_U64(pmem);
  atomic_printf("Core %d read: %lx\n", hartid, val);
}

void task1() {
  uint64_t hartid = riscv_mhartid();
  while(step != 1);
  uint64_t val = READ_U64(pmem);
  atomic_printf("Core %d read: %lx\n", hartid, val);
  barrier(NUM_CORES);
  riscv_cbo_inval((uint64_t)pmem);
  val = READ_U64(pmem);
  atomic_printf("Core %d read: %lx\n", hartid, val);
}

void task2(uint64_t hartid) {
  barrier(NUM_CORES);
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task2, task2};

int main() {
  uint64_t hartid = riscv_mhartid();
  if(hartid == 0){
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
  }
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}