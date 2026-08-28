// ipi: per-CPU ACLINT MSWI software-interrupt test (the AIA IMSIC mechanism
// has been removed).  Core 0 raises an IPI to core 1 through the MSWI
// register (CPU_SPACE(x) + TIMER_OFFSET + 0x10); each core polls its MSIP
// pending bit, clears the MSWI register and raises the IPI for the next core
// in the ring, for IPI_ITERATION full rings.

#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>
#include "ppu.h"
#include "csr.h"
#include "platform.h"

#define NUM_CORES      4
#define IPI_ITERATION  4

#define MSIP_BIT       3
#define MSWI_ADDR(x)   (CPU_SPACE(x) + TIMER_OFFSET + 0x10)

volatile uint8_t ipi_iter_cnt = 0;
volatile int     ipi_done     = 0;

static void raise_ipi(uint64_t hartid) {
  WRITE_U32(MSWI_ADDR(hartid), 1);
}

static void clear_ipi(uint64_t hartid) {
  WRITE_U32(MSWI_ADDR(hartid), 0);
}

int main() {
  uint64_t id = riscv_mhartid();
  if(id == 0) {
    for(int i = 1; i < NUM_CORES; i++) switch_on_core(i);
  }
  /* enable M-mode software interrupts on every core (mstatus.MIE stays off,
   * so the pending bit is polled instead of taking a trap) */
  csr_write(mie, csr_read(mie) | (1UL << MSIP_BIT));
  if(barrier(NUM_CORES)) return 1;

  if(id == 0) {
    printf("IPI test started!\n");
    raise_ipi(1);
  }

  /* ring: core i forwards the IPI to core (i+1) % NUM_CORES */
  while(!ipi_done) {
    while(!(csr_read(mip) & (1UL << MSIP_BIT))) {
      if(ipi_done) break;
    }
    if(ipi_done) break;
    clear_ipi(id);
    riscv_fence();
    atomic_printf("Core %lu: IPI raised!\n", id);
    if(id == 0) {
      ipi_iter_cnt++;
      if(ipi_iter_cnt >= IPI_ITERATION) {
        riscv_fence();
        ipi_done = 1;
        break;
      }
    }
    raise_ipi((id + 1) % NUM_CORES);
  }

  if(barrier(NUM_CORES)) return 1;
  if(id == 0) printf("IPI test passed!\n");
  /* second barrier: the verdict print must reach the UART before any core
   * halts (the sim finishes on the first halt) */
  if(barrier(NUM_CORES)) return 1;
  return 0;
}
