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
#define TEST_SIZE 32

volatile uint64_t step_lock = 0;
volatile int step = 0;
volatile bool busy[3] = {true, true, true};
volatile void *reg = (void *)0x90000000;
volatile void *pmem = (void *)0xa0000000;
volatile void *pmem2 = (void *)0xb0000000;

void ipi_handler() {
  uint64_t id = riscv_mhartid();
  asm volatile("sfence.vma");
  asm volatile("fence.i");
  clear_ipi(id);
}

int ipi_init() {
  csr_set(mie, MSIE);
  csr_set(mstatus, (0x1UL << 3));
  return 0;
}


void task0(uint64_t hartid) {
  for(int i=1; i<TEST_SIZE; i++){
    while(busy[0] || busy[1] || busy[2]){};
    busy[0] = true;
    busy[1] = true;
    busy[2] = true;
    riscv_fence();

    vm_huge_map((void *)(reg), (void *)(pmem2 + i * 0x200000), PTE_R | PTE_W | PTE_X | PTE_A | PTE_D, 1);
    for(int i=1;i<NUM_CORES;i++){
      raise_ipi(i);
    }
    step++;
    riscv_fence();
  }
}

void task1(uint64_t hartid) {
  for(int i = 0; i < TEST_SIZE; i++){
    while(step != i);
    uint64_t val = READ_U64(reg);
    if(val != (uint64_t)pmem2 + i * 0x200000){
      s_atomic_printf("Core 1 get wrong result %lx\n", val);
    }
    busy[0] = false;
    riscv_fence();
  }
  s_atomic_printf("Core 1 always get right result %lx\n");
}

void task2(uint64_t hartid) {
  int sum = 0;
  for(int i = 0; i < TEST_SIZE; i++){
    while(step != i);
    sum += READ_U64((uint64_t *)(reg - 0x1000) + i);
    busy[1] = false;
    riscv_fence();
  }
  if(sum == (TEST_SIZE - 1)*(TEST_SIZE)/2){
    s_atomic_printf("Core 2 get correct result\n");
  }else{
    s_atomic_printf("Core 2 get wrong result %d\n", sum);
  }

}

void task3(uint64_t hartid) {
  int sum = 0;
  for(int i = 0; i < TEST_SIZE; i++){
    while(step != i);
    sum += READ_U64((uint64_t *)(reg + 0x200000) + i);
    busy[2] = false;
    riscv_fence();
  }
  if(sum == (TEST_SIZE - 1)*(TEST_SIZE)/2){
    s_atomic_printf("Core 3 get correct result\n");
  }else{
    s_atomic_printf("Core 3 get wrong result %d\n", sum);
  }
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task2, task3};

void __attribute__((constructor)) s_main(int hartid){
  asm volatile("mv a0, %0" :: "r"(hartid));
  s_barrier(NUM_CORES, hartid);
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

    vm_map((void *)0x1000000, (void *)0x1000000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    for(uint64_t i = 0x80010000; i < 0x80030000; i+=0x1000){
      vm_map((void *)i, (void *)i, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    }
    // map 90000000 -> a0000000(2MiB page)
    // map 8ffff000 -> b0000000(4KiB page)
    // map 90200000 -> b0001000(4KiB page)
    vm_huge_map((void *)reg, (void *)pmem2, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D, 1);
    vm_map((void *)(reg - 0x1000), (void *)pmem, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    vm_map((void *)(reg + 0x200000), (void *)(pmem+0x1000), PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);

    for(int i = 0; i < TEST_SIZE; i++){
      WRITE_U64((uint64_t *)pmem + i, i);
    }
    for(int i = 0; i < TEST_SIZE; i++){
      WRITE_U64((uint64_t *)pmem + 512 + i, i);
    }

    for(int i = 0; i < TEST_SIZE; i++){
      WRITE_U64(pmem2 + i * 0x200000, (uint64_t)pmem2 + i * 0x200000);
    }
  }
  vm_enable(0x84000000);

  barrier(NUM_CORES);
  switch_mode(hartid, MODE_S, (uint64_t)&s_main);
}