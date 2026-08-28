// pwr-dyn-test: dynamic power retention test (AIA IMSIC replaced by the
// per-CPU ACLINT MSWI mechanism).  Core 0 powers on cores 1..3, then switches
// them to PWR_RET and wakes them with an MSWI software interrupt.

#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include "ppu.h"
#include "csr.h"
#include "platform.h"

#define NUM_CORES 4

#define MSIP_BIT  3
#define MSWI_ADDR(x) (CPU_SPACE(x) + TIMER_OFFSET + 0x10)

typedef enum {
  UNTEST = -1,
  ACCEPT = 0,
  DENY   = 1,
  DONE   = 2,
  ERROR  = 3,
  FINISH = 4
} flag_t;

volatile flag_t power_flags[NUM_CORES];

static void raise_ipi(uint64_t hartid) {
  WRITE_U32(MSWI_ADDR(hartid), 1);
}

static void clear_ipi(uint64_t hartid) {
  WRITE_U32(MSWI_ADDR(hartid), 0);
}

void other_core(uint64_t id) {
  while(power_flags[id] == UNTEST);
  power_flags[id] = DONE;
  riscv_fence();

  // wait for dyn test
  while(power_flags[id] != UNTEST);
  while(!(csr_read(mip) & (1UL << MSIP_BIT)));
  clear_ipi(id);
  riscv_fence();

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

  atomic_printf("[2. power dyn test]\n");
  for(int i = 1; i < NUM_CORES; i++) switch_power_mode(i, true, PWR_RET);
  for(int i = 1; i < NUM_CORES; i++) power_flags[i] = UNTEST;
  riscv_fence();
  for(int i = 1; i < NUM_CORES; i++) while((READ_U32(PWSR(i)) & 0x3) != PWR_RET);
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
