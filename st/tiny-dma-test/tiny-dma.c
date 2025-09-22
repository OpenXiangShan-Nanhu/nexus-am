#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include "cmo.h"
#include "ppu.h"
#include "plic.h"
#include "mtrap.h"
#include "csr.h"
#include "platform.h"
#include "dw_axi_dmac.h"

#define NUM_CORES 4
#define TEST_SIZE 16
#define TEST_GOLD (TEST_SIZE * (TEST_SIZE - 1) / 2)
#define LINE_SIZE ((TEST_SIZE * sizeof(uint64_t) + 64) / 64)
#define DMA_SIZE (((TEST_SIZE * sizeof(uint64_t)) + 31) & ~31)

volatile uint64_t step_lock = 0;
volatile uint64_t step = 0;
volatile uint64_t *reg = (uint64_t *)0x90000000;
volatile uint64_t *dma_src = (uint64_t *)0xa0000000;

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
  if(setup_intr(250, 7)) return 1;
  if(enable_intr(0, 250)) return 1;
  return 0;
}


void task0() {

  atomic_printf("init data: size %x\n", DMA_SIZE);
  for(int i = 0; i < TEST_SIZE; i++) {
    WRITE_U64(dma_src + i, i);
  }
  riscv_fence();
  atomic_printf("init data finish\n");


  tiny_dma_init(0);
  atomic_printf("start data transfer\n");
  tiny_dma_transfer_single_block(0,
     (uint64_t)dma_src, 
     (uint64_t)reg, 
     DMA_SIZE, 
     DWAXIDMAC_AX_CACHE_CACHEABLE, 
     DWAXIDMAC_AX_CACHE_CACHEABLE
    );


  step++; // 1
  riscv_fence();

}

void task1() {

  uint64_t id = riscv_mhartid();
  while (step != 1);
  int sum = 0;
  for (int i = 0; i < TEST_SIZE; i++) {
    sum += READ_U64(reg + i);
  }
  if (sum == TEST_GOLD){
    atomic_printf("Core %d get correct sum value %d!\n", id, sum);
  } else {
    atomic_printf("Core %d get wrong sum value %d!\n", id, sum);
  }
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task1, task1};
int main() {

  uint64_t hartid = riscv_mhartid();

  if(hartid == 0){
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    m_trap_handler_register(MEIP, m_trap_handler);
    setup_plic();
    ei_enable();
  }

  cpu[hartid]();
  barrier(NUM_CORES);
  return 0;
}