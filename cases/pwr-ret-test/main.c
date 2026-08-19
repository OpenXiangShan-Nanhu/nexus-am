#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include "ppu.h"
#include "clint.h"
#include "mtrap.h"
#include "ppu.h"
#include "csr.h"
#include "platform.h"

#define NUM_CORES 4

typedef enum {
  UNTEST = -1,
  ACCEPT = 0,
  DENY   = 1,
  DONE   = 2,
  ERROR  = 3,
  FINISH = 4
} flag_t;

volatile flag_t power_flags[NUM_CORES];

void ipi_handler() {
  if (imsic_ipi_claim() != IPI_EIID) {
    default_trap_handler();
    return;
  }
}

int ipi_init() {
  imsic_ipi_enable();
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MEIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
  return 0;
}


void other_core(uint64_t id) {
  if(m_trap_handler_register(MEIP, ipi_handler)) return ;
  ipi_init();

  power_flags[id] = DONE;
  riscv_fence();

  // wait for 2 ret deny test
  while(power_flags[id] != DENY && power_flags[id] != ACCEPT);
  if(power_flags[id] == ACCEPT) panic("Error!\n");
  power_flags[id] = DONE;
  riscv_fence();

  // wait for ret accrpt test
  riscv_wfi();
  while(power_flags[id] != ACCEPT);

  atomic_printf("Core %d test finish!\n", id);
  power_flags[id] = FINISH;
  riscv_fence();
}


void first_core() {
  atomic_printf("Core 0 is powered on!\n");
  for(int i = 1; i < NUM_CORES; i++) power_flags[i] = UNTEST;
  riscv_fence();

  atomic_printf("[1. power on test]\n");
  for(int i = 1; i < NUM_CORES; i++) power_flags[i] = switch_on_core(i);
  riscv_fence();
  for(int i = 1; i < NUM_CORES; i++) while(power_flags[i] != DONE);

  atomic_printf("[2. power ret deny test]\n");
  for(int i = 1; i < NUM_CORES; i++) power_flags[i] = switch_ret_core(i);
  riscv_fence();
  for(int i = 1; i < NUM_CORES; i++) while(power_flags[i] != DONE);

  atomic_printf("[3. power ret accept test]\n");
  for(volatile int i = 100; i > 0; i--) {}
  for(int i = 1; i < NUM_CORES; i++) power_flags[i] = switch_ret_core(i);
  for(int i = 1; i < NUM_CORES; i++) power_flags[i] = switch_on_core(i);
  riscv_fence();
  for(int i = 1; i < NUM_CORES; i++) raise_ipi(i);

  atomic_printf("Core 0 test finish!\n");
  power_flags[0] = FINISH;
  riscv_fence();
}


int main() {
  uint64_t hartid = riscv_mhartid();
  if(hartid >= NUM_CORES) {
    printf("ERROR: hart %d exceeds NUM_CORES (%d)\n", hartid, NUM_CORES);
    return -1;
  }
  else if(hartid == 0) first_core();
  else other_core(hartid);


  for(int i = 0; i < NUM_CORES; i++) while(power_flags[i] != FINISH);
  atomic_printf("All test finish\n");
  return 0;
}