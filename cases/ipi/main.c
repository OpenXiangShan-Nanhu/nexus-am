#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "clint.h"
#include "mtrap.h"
#include "ppu.h"
#include "csr.h"
#include "platform.h"
#include <stdint.h>

#define NUM_CORES 1
#define IPI_ITERATION 4

volatile uint8_t ipi_iter_cnt = 0;

void ipi_handler() {
  uint64_t id = riscv_mhartid();
  printf("Core %lu: IPI raised!\n", id);
  clear_ipi(id);
  if(id == 0) {
    ipi_iter_cnt ++;
    riscv_fence();
    if(ipi_iter_cnt >= IPI_ITERATION) return;
  }
  raise_ipi(0);
}

int ipi_init() {
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MSIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
  return 0;
}

int main() {
  uint64_t id = riscv_mhartid();
  printf("Core %lu is started!\n", id);
  if(id == 0) {
    if(m_trap_handler_register(MSIP, ipi_handler)) return 1;
  }
  ipi_init();
  if(id == 0) {
    printf("IPI test started!\n");
    raise_ipi(0);
    while(ipi_iter_cnt < IPI_ITERATION);
  } else {
    while(1) riscv_wfi();
  }
  printf("IPI test passed!\n");
}
