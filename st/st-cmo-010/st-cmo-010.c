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
    riscv_cbo_clean((uint64_t)pmem + hartid * 16);
  }
  barrier(NUM_CORES);
  for(int i = 0; i < TEST_SIZE; i++){
    riscv_cbo_inval((uint64_t)pmem + hartid * 16);
  }
  barrier(NUM_CORES);
  for(int i = 0; i < TEST_SIZE; i++){
    riscv_cbo_clean((uint64_t)pmem + hartid * 16);
  }


  barrier(NUM_CORES);
  for(int i = 0; i < CACHE_LINE; i+=8){
    uint64_t val = READ_U64((uint64_t)pmem + i);
    if(val != (uint64_t)pmem + i) {
      atomic_printf("Core %d data error: %d, %lx\n", hartid, i, val);
    }
  }
  atomic_printf("Core %d check done\n", hartid);
}

void task1() {
  uint64_t hartid = riscv_mhartid();
  for(int i = 0; i < TEST_SIZE; i++){
    riscv_cbo_clean((uint64_t)pmem + hartid * 16);
  }
  barrier(NUM_CORES);
  for(int i = 0; i < TEST_SIZE; i++){
    riscv_cbo_inval((uint64_t)pmem + hartid * 16);
  }
  barrier(NUM_CORES);
  for(int i = 0; i < TEST_SIZE; i++){
    riscv_cbo_inval((uint64_t)pmem + hartid * 16);
  }


  barrier(NUM_CORES);
  for(int i = 0; i < CACHE_LINE; i+=8){
    uint64_t val = READ_U64((uint64_t)pmem + i);
    if(val != (uint64_t)pmem + i) {
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

  }

  uint64_t addr1 = (uint64_t)pmem + hartid * 16;
  uint64_t addr2 = (uint64_t)pmem + hartid * 16 + 8;
  WRITE_U64(addr1, addr1);
  WRITE_U64(addr2, addr2);
  
  barrier(NUM_CORES);
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}