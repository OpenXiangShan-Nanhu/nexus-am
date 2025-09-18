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
#define TEST_SIZE 0x1000

volatile uint64_t step = 0;
volatile void *pmem = (void *)0x90000000;

extern uint64_t atomic_add(volatile uint64_t *addr, uint64_t adder);


void ipi_handler() {
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %d ipi handler\n", id);
  riscv_cbo_flush((uint64_t)pmem);
  clear_ipi(id);
}

int ipi_init() {
  csr_set(mie, MSIE);
  csr_set(mstatus, (0x1UL << 3));
  return 0;
}


void task0() {
  while(step != 1){
    raise_ipi(0);
    for(volatile int i = 0; i < 1000; i++);
  }
}

void task1() {
  for(int i = 0; i < TEST_SIZE; i++){
    atomic_add((uint64_t *)pmem, 1);
  }
  atomic_add(&step, 1);
  uint64_t result = READ_U64(pmem);
  atomic_printf("Core %d task1 result: 0x%lx\n", riscv_mhartid(), result);
}


void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, empty, empty};

int main() {
  uint64_t hartid = riscv_mhartid();
  if(hartid == 0) {
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    m_trap_handler_register(MSIP, ipi_handler);
    ipi_init();
  }
  barrier(NUM_CORES);
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}