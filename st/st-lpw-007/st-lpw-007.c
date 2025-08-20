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
volatile int *reg = (int *)0x90000000;

void task0() {

  for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);

  while(step != 1);
  for(volatile int i = 0; i < 100; i++);
  
  switch_off_core(1);
  switch_on_core(1);
}

void task1() {

  if(step != 0) { // run at second power-on
    return;
  }

  for(int i = 0; i < 100; i++){
    WRITE_U64(reg + i * 0x40, i);
  }

  step++; // 1
  riscv_cbo_flush((uint64_t)&step);
  riscv_wfi();
}

void task2(){
  while(step != 1);

  int sum = 0;
  atomic_printf("Core 2 snoop core 1\n");
  for(volatile int i = 0; i < 2000; i++);
  for(int i = 0; i < 100; i++){
    sum += READ_U64(reg + i * 0x40);
  }

  if(sum == 4950){
    atomic_printf("Core 2 get correct value!\n");
  }
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task2, empty};

int main() {
  uint64_t hartid = riscv_mhartid();
  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}