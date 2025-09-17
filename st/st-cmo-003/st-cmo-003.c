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
#define TEST_SIZE 1000


volatile uint64_t step = 0;
volatile void *pmem = (void *)0x90000000;

extern uint64_t atomic_add(volatile uint64_t *addr, uint64_t adder);

void task0() {

  while(step != 3){
    riscv_cbo_clean((uint64_t)pmem);
  }
  uint64_t result = READ_U64(pmem);

  if(result == TEST_SIZE * 3) {
    printf("Result correct: %d\n", result);
  } else {
    printf("Result error: %d\n", result);
  }
}

void task1() {
  for(int i = 0; i < TEST_SIZE; i++) {
    atomic_add(pmem, 1);
  }
  atomic_add(&step, 1);
}

void empty(){}
void (*cpux[NUM_CORES])() = {task0, task1, empty, empty};
void (*cpuy[NUM_CORES])() = {task0, task1, task1, empty};
void (*cpu[NUM_CORES])() = {task0, task1, task1, task1};

int main() {
  uint64_t hartid = riscv_mhartid();
  if(hartid == 0) {
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
  }
  barrier(NUM_CORES);
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}