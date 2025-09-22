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
#include "plic.h"
#include "csr.h"
#include "vmm.h"
#include "platform.h"
#include "dw_axi_dmac.h"

#define NUM_CORES 4

volatile int dma_cnt = 0;
volatile int step = 0;
volatile void *reg = (int *)0x90000000;
volatile void *dma_src = (int *)0xa0000000;

void ipi_handler() {
  uint64_t id = riscv_mhartid();
  s_atomic_printf("Core %d get ipi and sfence.vma\n", id);
  asm volatile("sfence.vma");
  asm volatile("fence.i");
  clear_ipi(id);
}

int ipi_init() {
  csr_set(mie, MSIE);
  csr_set(mstatus, (0x1UL << 3));
  return 0;
}

void m_trap_handler() {
  uint64_t id = riscv_mhartid();
  uint32_t ctx = id * 2;

  uint32_t intr = READ_U32(CTX_COMP_REG(ctx));
  uint32_t dma_id = intr - 250;

  atomic_printf("Get external interrupt %d!\n", intr);

  if(dma_id >= 0 && dma_id < 6){
    default_dma_handler(dma_id);
  }
  WRITE_U32(CTX_COMP_REG(ctx), intr);
  dma_cnt++;
  step++;
  riscv_fence();
}

void ei_enable() {
  csr_set(mie, MEIE);
  csr_set(mstatus, (0x1UL << 3));
}

int setup_plic() {
  // plic_init(1);
  if(setup_context(0, 3)) return 1;
  for (int i = 250; i < 256; i++){
    if(setup_intr(i, 7)) return 1;
    if(enable_intr(0, i)) return 1;
  }
  return 0;
}


void task0(uint64_t hartid) {
  while(step != 1);
  for(int i = 0; i < 6; i ++){
    vm_map((void *)reg + i * 0x1000, (void *)0x98000000 + i * 0x1000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }
  for(int i = 0; i < 6; i ++){
    tiny_dma_transfer_single_block(i,
     (uint64_t)dma_src + i * 0x1000,
     (uint64_t)0x98000000 + i * 0x1000,
     0x1000, 
     DWAXIDMAC_AX_CACHE_CACHEABLE, 
     DWAXIDMAC_AX_CACHE_CACHEABLE
    );
  }
  raise_ipi(1);
}

void task1(uint64_t hartid) {

  // refill tlb
  for(int i = 0; i < 6; i++){
    READ_U64((void *)reg + i * 0x1000);
  }
  step++; // 1
  riscv_fence();
  riscv_wfi();
  for(int i = 0; i < 512*6; i++){
    uint64_t val = READ_U64((void *)reg + i * 8);
    if(val != (uint64_t)dma_src + i * 8){
      s_atomic_printf("Core 1 get wrong result, expect: 0x%lx, get: 0x%lx\n", (uint64_t)dma_src + i * 8, val);
    }
  }
  s_atomic_printf("Core 1 read all data, correct!\n");

}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, empty, empty};

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

    // map 90000000 -> 90000000 
    vm_init(0x84000000);
    vm_map((void *)0x1000000, (void *)0x1000000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    for(int i = 0; i < 6; i ++){
      vm_map((void *)reg + i * 0x1000, (void *)reg + i * 0x1000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
      vm_map((void *)0x50070000 + i * 0x10000, (void *)0x50070000 + i * 0x10000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    }
    for(uint64_t addr = 0x80010000; addr < 0x80030000; addr += 0x1000){
      vm_map((void *)addr, (void *)addr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    }

    // init data
    for(int i = 0; i < 512 * 6; i++){
      WRITE_U64((uint64_t *)dma_src + i, (uint64_t)dma_src + i * 8);
    }

    // init dma
    m_trap_handler_register(MEIP, m_trap_handler);
    setup_plic();
    ei_enable();
    for(int i = 0; i < 6; i++){
      tiny_dma_init(i);
    }
  }
  vm_enable(0x84000000);

  barrier(NUM_CORES);
  switch_mode(hartid, MODE_S, (uint64_t)&s_main);
}