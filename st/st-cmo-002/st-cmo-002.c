#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include <stdint.h>
#include "cmo.h"
#include "ppu.h"
#include "plic.h"
#include "clint.h"
#include "mtrap.h"
#include "csr.h"
#include "platform.h"
#include "dw_axi_dmac.h"

#define NUM_CORES 4
#define TEST_SIZE 0x1000
#define DMAC_INTR_SOURCE_BASE 250

volatile uint64_t step = 0;
volatile void *pmem = (void *)0x90000000;
volatile void *dma_mem = (void *)0xa0000000;

extern void atomic_add(uint64_t *addr, uint64_t val);

void m_trap_handler() {
  uint32_t intr = READ_U32(CTX_COMP_REG(0));
  atomic_printf("Get external interrupt %d!\n", intr);
  default_dma_handler(0);
  WRITE_U32(CTX_COMP_REG(0), intr);
  atomic_add((uint64_t *)&step, 1);
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


void task0() {
  WRITE_U64(pmem, 0xdeedbeef);
  riscv_fence();
  atomic_add((uint64_t *)&step, 1);
  while(step!=4);
  riscv_cbo_inval((uint64_t)pmem);

  barrier(NUM_CORES);

  tiny_dma_transfer_single_block(0,
    (uint64_t)dma_mem,
    (uint64_t)pmem,
    32,
    DWAXIDMAC_AX_CACHE_CACHEABLE, 
    DWAXIDMAC_AX_CACHE_CACHEABLE
  );
  riscv_wfi();

}

void task1() {

  uint64_t hartid = riscv_mhartid();

  while(step != 1);
  READ_U64(pmem);
  atomic_add((uint64_t *)&step, 1);

  barrier(NUM_CORES);

  while (step != 5);
  uint64_t val = READ_U64(pmem);

  if(val != 0x88888888){
    atomic_printf("ERROR: Core %d read: %lx\n", hartid, val);
  } else {
    atomic_printf("PASS: Core %d read: %lx\n", hartid, val);
  }
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task1, task1};

int main() {
  uint64_t hartid = csr_read(mhartid);


  if(hartid == 0) {
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    tiny_dma_init(0);
    m_trap_handler_register(MEIP, m_trap_handler);
    setup_plic();
    ei_enable();
    WRITE_U64(dma_mem, 0x88888888);
  }
  barrier(NUM_CORES);
  cpu[hartid]();
  barrier(NUM_CORES);

  return 0;
}