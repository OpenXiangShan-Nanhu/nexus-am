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
#define TEST_SIZE 0x100
#define ALL_SIZE (TEST_SIZE * 8)
#define CACHE_LINE 64


volatile uint64_t step = 0;
volatile void *pmem = (void *)0x90000000;

extern uint64_t atomic_add(volatile uint64_t *addr, uint64_t adder);

void task0() {
  uint64_t hartid = riscv_mhartid();
  for(int i = 0; i < TEST_SIZE; i++){
    riscv_cbo_clean((uint64_t)pmem + ((i*CACHE_LINE)%ALL_SIZE));
  }
  barrier(NUM_CORES);
  for(int i = 0; i < TEST_SIZE; i++){
    uint64_t val = READ_U64((uint64_t)pmem + i * 8);
    if(val != (uint64_t)pmem + i * 8) {
      atomic_printf("Core %d data error: %d, %lx\n", hartid, i, val);
    }
  }
  atomic_printf("Core %d check done\n", hartid);
}

void task1() {
  uint64_t hartid = riscv_mhartid();
  for(int i = 0; i < TEST_SIZE; i++){
    riscv_cbo_inval((uint64_t)pmem + ((i*CACHE_LINE)%ALL_SIZE));
  }
  barrier(NUM_CORES);
  for(int i = 0; i < TEST_SIZE; i++){
    uint64_t val = READ_U64((uint64_t)pmem + i * 8);
    if(val != (uint64_t)pmem + i * 8) {
      atomic_printf("Core %d data error: %d, %lx\n", hartid, i, val);
    }
  }
  atomic_printf("Core %d check done\n", hartid);
}


void empty(){}
void (*cpu[NUM_CORES])() = {task0, task0, task0, task0};

int main() {
  uint64_t hartid = riscv_mhartid();
  if(hartid == 0) {
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    for(int i = 0; i < TEST_SIZE; i++) {
      uint64_t addr = (uint64_t)pmem + i * 8;
      WRITE_U64(addr, addr);
    }
  }

  // refill cache
  if(hartid > 0) {
    for(int i = 0; i < ALL_SIZE; i+=CACHE_LINE) {
      READ_U64((uint64_t)pmem + i * 8);
    }
  }

  barrier(NUM_CORES);
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}