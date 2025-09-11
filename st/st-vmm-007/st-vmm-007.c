#include <am.h>
#include <xsextra.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include "riscv.h"
#include "clint.h"
#include "ppu.h"
#include "mtrap.h"
#include "riscv.h"
#include "strap.h"
#include "csr.h"
#include "vmm.h"
#include "platform.h"

#define NUM_CORES 4

volatile uint64_t step_lock = 0;
volatile int step = 0;
volatile int *reg_9 = (int *)0x90000000;
volatile int *reg_a = (int *)0xa0000000;

void task0(uint64_t hartid) {
  int sum = 0;
  for(int i = 0; i < 256; i++) {
    vm_map((void *)reg_9, (void *)reg_9 + i * 2 * 0x1000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    asm volatile("sfence.vma");
    sum += READ_U64(reg_9);
  }
  if(sum == 510 * 256 / 2)
    s_atomic_printf("Core %d get correct result %d\n", hartid, sum);
  else
    s_atomic_printf("Core %d get wrong result %d\n", hartid, sum);
}

void task1(uint64_t hartid) {
  int sum = 0;
  for(int i = 0; i < 256; i++) {
    vm_map((void *)reg_a, (void *)reg_9 + i * 2 * 0x1000 + 0X1000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    asm volatile("sfence.vma");
    sum += READ_U64(reg_a);
  }
  
  if(sum == 512 * 256 / 2)
    s_atomic_printf("Core %d get correct result %d\n", hartid, sum);
  else
    s_atomic_printf("Core %d get wrong result %d\n", hartid, sum);
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, empty, empty};

void __attribute__((constructor)) s_main(int hartid){
  asm volatile("mv a0, %0" :: "r"(hartid));
  cpu[hartid]();
  s_barrier(NUM_CORES, hartid);
  _halt(0);
}

int main() {

  uint64_t hartid = riscv_mhartid();
  m_trap_handler_register(INS_PAGE_FAULT, default_page_fault_handler);
  m_trap_handler_register(LOAD_PAGE_FAULT, default_page_fault_handler);
  m_trap_handler_register(STORE_PAGE_FAULT, default_page_fault_handler);

  if (hartid == 0) {
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    vm_init(0x84000000);

    atomic_printf("Init data\n");
    for(int i = 0; i < 512; i++) {
      WRITE_U64((void*)reg_9 + i * 0x1000, i);
    }
    atomic_printf("Init data finish\n");
  }
  vm_enable(0x84000000);

  barrier(NUM_CORES);
  //switch_mode(hartid, MODE_S, (uint64_t)&s_main);
  switch_mode(hartid, MODE_S, (uint64_t)&s_main);
}