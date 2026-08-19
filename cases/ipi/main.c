#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "clint.h"
#include "mtrap.h"
#include "ppu.h"
#include "csr.h"
#include "platform.h"
#include <stdint.h>

#define NUM_CORES 4
#define IPI_ITERATION 4

volatile uint8_t ipi_iter_cnt = 0;

void ipi_handler() {
  if (imsic_ipi_claim() != IPI_EIID) {
    default_trap_handler();
    return;
  }
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %lu: IPI raised!\n", id);
  if(id == 0) {
    ipi_iter_cnt ++;
    riscv_fence();
    if(ipi_iter_cnt >= IPI_ITERATION) return;
  }
  raise_ipi((id + 1) % NUM_CORES);
}

int ipi_init() {
  imsic_ipi_enable();
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MEIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
  return 0;
}

int main() {
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %lu is started!\n", id);
  if(id == 0) {
    if(m_trap_handler_register(MEIP, ipi_handler)) return 1;
    for(int i = 0; i < NUM_CORES; i++) switch_on_core(i);
  }
  ipi_init();
  if(barrier(NUM_CORES)) return 1;
  if(id == 0) {
    printf("IPI test started!\n");
    raise_ipi((id + 1) % NUM_CORES);
    while(ipi_iter_cnt < IPI_ITERATION) riscv_wfi();
  } else {
    while(1) riscv_wfi();
  }
  printf("IPI test passed!\n");
}