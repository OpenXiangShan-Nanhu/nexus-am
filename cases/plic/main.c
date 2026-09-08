#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "plic.h"
#include "mtrap.h"
#include "ppu.h"
#include "csr.h"
#include "platform.h"
#include "intr_gen.h"
#include <stdint.h>

#define NUM_CORES 4
#define ITERATION 1

_Static_assert(PLIC_BASE_ADDR == 0x3c000000UL, "PLIC test requires the new PLIC base");
_Static_assert(INTR_PRIO_REG(1 + 1) == 0x3c000008UL, "Unexpected PLIC priority stride");
_Static_assert(INTR_EN_REG(2 + 4, NR_INTR / 32) == 0x3c002320UL,
               "Unexpected PLIC enable map");
_Static_assert(CTX_THD_REG(0) == 0x3c200000UL &&
               CTX_COMP_REG((NUM_CORES - 1) * 2) == 0x3c206004UL,
               "Unexpected PLIC context map");
_Static_assert(PWPR(0) == 0x01001000UL && PWSR(3) == 0x010c1004UL,
               "Unexpected PPU startup map");

volatile uint8_t iter_cnt = 0;

void intr_handler() {
  uint64_t id = riscv_mhartid();
  uint32_t ctx = id * 2;

  uint32_t intr = READ_U32(CTX_COMP_REG(ctx));
  clear_ext_intr(intr);
  WRITE_U32(CTX_COMP_REG(ctx), intr);
  
  atomic_printf("Core %lu get external interrupt %d!\n", id, intr);
  if(intr == NR_INTR) {
    iter_cnt ++;
    riscv_fence();
    if(iter_cnt >= ITERATION) return;
  }
  raise_ext_intr((intr + 1) % (NR_INTR + 1));
}

void enable_external_intr() {
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MEIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
}

int setup_plic() {
  plic_init(NUM_CORES);
  for(int i = 1; i <= NR_INTR; i++) if(setup_intr(i, 7)) return 1;
  for(int i = 0; i < NUM_CORES; i++) {
    uint32_t ctx = i * 2;
    if(setup_context(ctx, 3)) return 1;
    for(int j = 1; j <= NR_INTR; j ++) {
      if(j % NUM_CORES == i) {
        if(enable_intr(ctx, j)) return 1;
      }
    }
  }
  return 0;
}

int main() {
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %lu is started!\n", id);
  if(id == 0) {
    printf("PLIC map: base=0x%lx enable[0]=0x%lx claim[0]=0x%lx PPU[0]=0x%lx interrupts=1..%d\n",
           PLIC_BASE_ADDR, INTR_EN_REG(0, 0), CTX_COMP_REG(0), PPU_ADDR(0), NR_INTR);
    if(setup_plic()) return 1;
    printf("PLIC is initialized!\n");
    if(m_trap_handler_register(MEIP, intr_handler)) return 1;
    for(int i = 0; i < NUM_CORES; i++) switch_on_core(i);
  }
  enable_external_intr();
  if(barrier(NUM_CORES)) return 1;
  if(id == 0) {
    printf("PLIC test started!\n");
    raise_ext_intr(id + 1);
    while(iter_cnt < ITERATION) riscv_wfi();
  } else {
    while(1) riscv_wfi();
  }
  printf("PLIC test passed!\n");
}