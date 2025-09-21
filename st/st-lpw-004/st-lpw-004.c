#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include <stdint.h>
#include "intr_gen.h"
#include "vmm.h"
#include "ppu.h"
#include "clint.h"
#include "mtrap.h"
#include "csr.h"
#include "platform.h"
#include "riscv.h"

#define NUM_CORES 4
#define TEST_SIZE 16

volatile int step = 0;
volatile void *pmem = (void *)0x90000000;

void ipi_handler() {
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %d get ipi!\n", id);
  asm volatile("sfence.vma");
  asm volatile("fence.i");
  clear_ipi(id);
}

int ipi_init() {
  csr_set(mie, MSIE);
  csr_set(mstatus, (0x1UL << 3));
  return 0;
}

void task0() {
  while(step != 1);
  WRITE_U64(0x84009000, 0); // invalid va 0x90000000 mapping
  riscv_fence();
  for(volatile int i = 0; i < 1000; i++);

  raise_ipi(1);

  while(step != 2);
}

void __attribute__((constructor)) task1(int hartid){
  for(int i = 0; i < TEST_SIZE/2; i++){
    uint64_t val = READ_U64(pmem + i * 8);
    if(val != (uint64_t)pmem + i * 8){
      s_atomic_printf("Core %d get wrong result %lx\n", hartid, val);
    } else {
      s_atomic_printf("Core %d get right result %lx\n", hartid, val);
    }
  }
  step++;
  riscv_fence();
  riscv_wfi();

  for(int i = TEST_SIZE/2; i < TEST_SIZE; i++){
    uint64_t val = READ_U64(pmem + i * 8);
    if(val != (uint64_t)pmem + i * 8){
      s_atomic_printf("Core %d get wrong result %lx\n", hartid, val);
    } else {
      s_atomic_printf("Core %d get right result %lx\n", hartid, val);
    }
  }
  step++;
  riscv_fence();
  riscv_wfi();
}

void empty(){
  while(true){
    riscv_wfi();
  }
}
void (*cpu[NUM_CORES])() = {task0, empty, empty, empty};

int main() {
  uint64_t hartid = riscv_mhartid();

  if(hartid == 0){
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    switch_power_mode(1, true, PWR_RET);

    for(volatile int i = 0; i < TEST_SIZE; i++){
      WRITE_U64((void *)pmem + i * 8, (uint64_t)pmem + i * 8);
    }
    riscv_fence();

    vm_init(0x84000000);
    vm_map((void *)0x1000000, (void *)0x1000000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    vm_map((void *)pmem, (void *)pmem, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }
  vm_enable(0x84000000);
  barrier(NUM_CORES);
  if(hartid == 1){
    ipi_init();
    m_trap_handler_register(MSIP, ipi_handler);
    m_trap_handler_register(INS_PAGE_FAULT, default_page_fault_handler);
    m_trap_handler_register(LOAD_PAGE_FAULT, default_page_fault_handler);
    m_trap_handler_register(STORE_PAGE_FAULT, default_page_fault_handler);
    switch_mode(hartid, MODE_S, (uint64_t)&task1);
  }

  cpu[hartid]();
  return 0;
}