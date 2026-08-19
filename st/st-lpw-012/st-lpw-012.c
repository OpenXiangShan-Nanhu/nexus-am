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
volatile void *reg = (void *)0x90000000;

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

  for(int i=0; i<TEST_SIZE; i++){
    for(volatile int j = 0; j<100;j++);
    uint64_t val = READ_U64(reg + i * 0x40);
    if(val != (uint64_t)reg + i * 0x40){
      atomic_printf("Core 0 get wrong value 0x%lx at address 0x%lx\n", val, reg + i * 0x40);
    } else {
      atomic_printf("Core 0 get correct value 0x%lx at address 0x%lx\n", val, reg + i * 0x40);
    }
    step++;
    riscv_fence();
  }
  raise_ipi(1);
}

void task1() {
  riscv_wfi();
  atomic_printf("Core 1 wake up %d times\n", step);
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, empty, empty};

int main() {

  uint64_t hartid = riscv_mhartid();

  if(hartid == 0){
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    switch_power_mode(1, true, PWR_RET);
  }

  if(hartid == 1){
    ipi_init();
    if(m_trap_handler_register(MEIP, ipi_handler)) return -1;
    for(int i = 0; i < TEST_SIZE; i++){
      WRITE_U64(reg + i * 0x40, (uint64_t)reg + i * 0x40);
    }
    riscv_fence();
  }

  barrier(NUM_CORES);
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}