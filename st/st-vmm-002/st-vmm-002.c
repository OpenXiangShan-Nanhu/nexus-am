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
volatile int *reg = (int *)0x90000000;

void task0(uint64_t hartid) {
  // 2. write 0xdeedbeef to vaddr 0x90000000
  s_atomic_printf("Core %d write 0xdeedbeef to vaddr 0x90000000\n", hartid);
  WRITE_U64(reg, 0xdeedbeef);
  step++; // 1
  riscv_fence();

  // 4. modifiy map 90000000 -> 98000000
  //    and write 0x12345678 vaddr 0x90000000
  while(step != 4);
  vm_map((void *)reg, (void *)0x98000000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  asm volatile("sfence.vma");
  s_atomic_printf("Core %d write 0x12345678 to vaddr 0x90000000\n", hartid);
  WRITE_U64(reg, 0x12345678);
  step++; // 5
  riscv_fence();

  // 5. read 0x90000000
  uint64_t val = READ_U64(reg);
  s_atomic_printf("Core %d read 0x%lx from 0x%lx\n", hartid, val, reg);
}

void task1(uint64_t hartid) {
  // 3. read 0x90000000
  while(step != 1);
  uint64_t val = READ_U64(reg);
  s_atomic_printf("Core %d read 0x%lx from 0x%lx\n", hartid, val, reg);
  compare_and_swap(&step_lock, 0, 1);
  step++; // 4
  step_lock = 0;
  riscv_fence();

  // 5. read 0x90000000
  while(step != 5);
  val = READ_U64(reg);
  s_atomic_printf("Core %d read 0x%lx from 0x%lx\n", hartid, val, reg);
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task1, task1};

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

    // 1. map 90000000 -> 90000000
    vm_map((void *)reg, (void *)reg, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }
  vm_enable(0x84000000);

  barrier(NUM_CORES);
  //switch_mode(hartid, MODE_S, (uint64_t)&s_main);
  switch_mode(hartid, MODE_S, (uint64_t)&s_main);
}