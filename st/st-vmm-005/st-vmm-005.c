#include <am.h>
#include <stdint.h>
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

#define TEST_SIZE 64
#define TEST_GOLD ((TEST_SIZE * (TEST_SIZE - 1)) / 2)

extern uint64_t atomic_add(volatile uint64_t *addr, uint64_t adder);

volatile uint64_t step_lock = 0;
volatile uint64_t step = 0;
volatile int finish = 0;
volatile int *reg = (int *)0x90000000;

void ipi_handler() {
  uint64_t id = riscv_mhartid();
  // s_atomic_printf("Core %d get ipi and sfence.vma\n", id);
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
  for(volatile int i = 0; i < 1000; i++);
  for(int i = 0; i < TEST_SIZE; i++){
    vm_map((void *)reg, (void *)reg + i * 0x1000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    asm volatile("sfence.vma");
    raise_ipi(1);
    raise_ipi(2);
    raise_ipi(3);
    while(step != 3*(i+1));
  }
}

void task1(uint64_t hartid) {
  int sum = 0;
  for(int i = 0; i < TEST_SIZE; i++){
    riscv_wfi();
    sum += READ_U64(reg);
    atomic_add((uint64_t *)&step, 1);
  }
  
  if (sum == TEST_GOLD)
    s_atomic_printf("Core %d get correct result %d\n", hartid, sum);
  else
    s_atomic_printf("Core %d get wrong result %d\n", hartid, sum);

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

  if(hartid > 0){
    m_trap_handler_register(MSIP, ipi_handler);
    ipi_init();
  }

  if (hartid == 0) {
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    vm_init(0x84000000);

    atomic_printf("vm_map 0x1000000 and 0x80010000\n");
    vm_map((void *)0x1000000, (void *)0x1000000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    for(void *va = (void *)0x80010000; va < (void *)0x80030000; va += 0x1000){
      vm_map(va, va, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    }
    atomic_printf("vm_init done\n");
  }
  vm_enable(0x84000000);


  // init data
  for(int i = 0; i < TEST_SIZE; i++){
    WRITE_U64((void *)reg + i * 0x1000, i);
  }

  barrier(NUM_CORES);
  switch_mode(hartid, MODE_S, (uint64_t)&s_main);
}