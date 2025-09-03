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

volatile int step = 0;
volatile int *reg = (int *)0x90000000;

void ipi_handler() {
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %d get ipi and sfence.vma\n", id);
  asm volatile("sfence.vma");
  asm volatile("fence.i");
  clear_ipi(id);
}

int ipi_init() {
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MSIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
  return 0;
}


void task0(uint64_t hartid) {
  // 2. write 0xdeedbeef to vaddr 0x90000000
  WRITE_U64(reg, 0xdeedbeef);
  step++; // 1
  riscv_fence();

  // 4. modifiy map 90000000 -> 98000000
  //    and write 0x12345678 vaddr 0x90000000
  vm_map((void *)reg, (void *)0x98000000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  for(int i = 1; i < NUM_CORES; i++) { raise_ipi(i); }
  WRITE_U64(reg, 0x12345678);
  step++; // 2
  riscv_fence();
}

void task1(uint64_t hartid) {
  // 3. read 0x90000000
  while(step != 1);
  uint64_t val = READ_U64(reg);
  s_atomic_printf("Core %d read 0x%lx from 0x%lx\n", hartid, val, reg);

  // 5. read 0x90000000
  while(step != 2);
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
  m_trap_handler_register(MSIP, ipi_handler);
  ipi_init();

  if (hartid == 0) {
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    vm_init(0x84000000);

    // 1. map 90000000 -> 90000000
    vm_map((void *)reg, (void *)reg, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }


  barrier(NUM_CORES);
  //switch_mode(hartid, MODE_S, (uint64_t)&s_main);
  m_switch_mode(hartid, MODE_S, (uint64_t)&s_main);
}