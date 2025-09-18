#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include <stdint.h>
#include "cmo.h"
#include "plic.h"
#include "clint.h"
#include "mtrap.h"
#include "csr.h"
#include "platform.h"
#include "dw_axi_dmac.h"

#define TEST_SIZE 0x1000
#define DMAC_INTR_SOURCE_BASE 250


volatile uint64_t step = 0;
volatile void *cpu_mem1 = (void *)0x90000000;
volatile void *cpu_mem2 = (void *)0xa0000000;
volatile void *dma_mem1 = (void *)0xb0000000;
volatile void *dma_mem2 = (void *)0xc0000000;

void m_trap_handler() {
  uint32_t intr = READ_U32(CTX_COMP_REG(0));
  atomic_printf("Get external interrupt %d!\n", intr);
  default_dma_handler(0);
  WRITE_U32(CTX_COMP_REG(0), intr);
}

void ei_enable() {
  csr_set(mie, MEIE);
  csr_set(mstatus, (0x1UL << 3));
}

int setup_plic() {
  if(setup_context(0, 3)) return 1;
  if(setup_intr(DMAC_INTR_SOURCE_BASE, 7)) return 1;
  if(enable_intr(0, DMAC_INTR_SOURCE_BASE)) return 1;
  return 0;
}

int main() {
  m_trap_handler_register(MEIP, m_trap_handler);
  setup_plic();
  ei_enable();
  tiny_dma_init(0);

  // cpu write, dma read
  for(int i = 0; i < TEST_SIZE; i++){
    uint64_t addr = (uint64_t)cpu_mem1 + i * 8;
    WRITE_U64(addr, addr);
  }

  riscv_fence();
  for(int i = 0; i < TEST_SIZE * 8; i+=64){
    uint64_t addr = (uint64_t)cpu_mem1 + i;
    riscv_cbo_clean(addr);
  }

  tiny_dma_transfer_single_block(0,
    (uint64_t)cpu_mem1,
    (uint64_t)dma_mem1,
    TEST_SIZE * 8, 
    DWAXIDMAC_AX_CACHE_CACHEABLE, 
    DWAXIDMAC_AX_CACHE_CACHEABLE
  );
  riscv_wfi();

  // dma write, cpu read
  for(int i = 0; i < TEST_SIZE; i++){
    uint64_t addr = (uint64_t)dma_mem2 + i * 8;
    WRITE_U64(addr, addr);
  }
  riscv_fence();

  tiny_dma_transfer_single_block(0,
    (uint64_t)dma_mem2,
    (uint64_t)cpu_mem2,
    TEST_SIZE * 8, 
    DWAXIDMAC_AX_CACHE_CACHEABLE, 
    DWAXIDMAC_AX_CACHE_CACHEABLE
  );
  riscv_wfi();


  //check dma_mem1
  for(int i = 0; i < TEST_SIZE; i++){
    uint64_t addr = (uint64_t)dma_mem1 + i * 8;
    uint64_t expect = (uint64_t)cpu_mem1 + i * 8;
    uint64_t val = READ_U64(addr);
    if(val != expect){
      atomic_printf("Error: expect %lx, got %lx\n", expect, val);
    }
  }

  //check cpu_mem2
  for(int i = 0; i < TEST_SIZE; i++){
    uint64_t addr = (uint64_t)cpu_mem2 + i * 8;
    uint64_t expect = (uint64_t)dma_mem2 + i * 8;
    uint64_t val = READ_U64(addr);
    if(val != expect){
      atomic_printf("Error: expect %lx, got %lx\n", expect, val);
    }
  }
  atomic_printf("Test pass\n");

  return 0;
}