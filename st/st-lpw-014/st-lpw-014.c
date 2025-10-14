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
#define TEST_SIZE 10

volatile int step = 0;
volatile void *pmem1 = (void *)0x90000000;
volatile void *pmem2 = (void *)0xa0000000;

void ipi_handler() {
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %d get ipi!\n", id);
  clear_ipi(id);
}

int ipi_init() {
  csr_set(mie, MSIE);
  csr_set(mstatus, (0x1UL << 3));
  return 0;
}

void task0() {
  for (volatile int i =0; i < 0x100; i++);
  switch_ret_core(2);
  uint64_t val1 = READ_U64(pmem1);
  atomic_printf("Core 0 get pmem1 value 0x%lx\n", val1);

  step++;
  riscv_fence();

  uint64_t val2 = READ_U64(pmem2);
  atomic_printf("Core 0 get pmem2 value 0x%lx\n", val2);

}

void task1() {
  riscv_wfi();
  atomic_printf("Core 1 wake up %d times\n", step);
}


void task2() {
  for(uint64_t addr = 0x80000000; addr < 0x80004000; addr += 0x40) {
    riscv_cbo_flush(addr);
  }
  riscv_wfi();
  atomic_printf("Core 2 wake up %d times\n", step);
}

void task3() {
  while(step != 1);
  for (volatile int i =0; i < 0x1000; i++);
  switch_on_core(2);
  raise_ipi(1);
  raise_ipi(2);
}


void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task2, task3};

int main() {

  uint64_t hartid = riscv_mhartid();
  ipi_init();
  m_trap_handler_register(MSIP, ipi_handler);


  if(hartid == 0){
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    switch_power_mode(1, true, PWR_RET);
  }

  if(hartid == 1){
    WRITE_U64(pmem1, (uint64_t)pmem1);
    riscv_fence();
  }

  if(hartid == 2){
    WRITE_U64(pmem2, (uint64_t)pmem2);
    riscv_fence();
  }

  barrier(NUM_CORES);
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}