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

_Static_assert(CLINT_BASE_ADDR == 0x38000000UL, "IPI requires the new CLINT base");
_Static_assert(MSIP_ADDR(1 + 2) == 0x3800000cUL, "Unexpected MSIP stride");
_Static_assert(MTIMECMP_ADDR(NUM_CORES - 1) == 0x38004018UL, "Unexpected MTIMECMP stride");
_Static_assert(TIMER_FREQ == 10000000UL, "CLINT timer must run at 10 MHz");
_Static_assert(PWPR(0) == 0x01001000UL && PWSR(3) == 0x010c1004UL,
               "Unexpected PPU startup map");
_Static_assert(CPU_SPACE(64) == 0x02000000UL, "CPU space must not wrap hart IDs");

volatile uint8_t ipi_iter_cnt = 0;

static int probe_mtimecmp() {
  int failed = 0;
  uint64_t expected[NUM_CORES];
  for(int phase = 0; phase < 3; phase++) {
    for(int i = 0; i < NUM_CORES; i++) {
      unsigned long addr = MTIMECMP_ADDR(i);
      // Keep every intermediate compare value far in the future, with distinct hart values.
      if(phase == 0) {
        expected[i] = 0xfffffffe12345678UL + i;
        WRITE_U64(addr, expected[i]);
      } else if(phase == 1) {
        expected[i] = (expected[i] & 0xffffffff00000000UL) | (0x89abcdefUL + i);
        WRITE_U32(addr, (uint32_t)expected[i]);
      } else {
        expected[i] = ((0xfffffffcUL - i) << 32) | (uint32_t)expected[i];
        WRITE_U32(addr + 4, (uint32_t)(expected[i] >> 32));
      }
    }
    riscv_fence();
    for(int i = 0; i < NUM_CORES; i++) {
      unsigned long addr = MTIMECMP_ADDR(i);
      uint64_t actual = READ_U64(addr);
      uint32_t lo = READ_U32(addr);
      uint32_t hi = READ_U32(addr + 4);
      if(actual != expected[i] || lo != (uint32_t)expected[i] || hi != (uint32_t)(expected[i] >> 32)) {
        printf("MTIMECMP phase %d hart %d @ 0x%lx: expected 0x%lx, got 0x%lx (hi=0x%x lo=0x%x)\n",
               phase, i, addr, (unsigned long)expected[i], (unsigned long)actual, hi, lo);
        failed = 1;
      }
    }
  }
  for(int i = 0; i < NUM_CORES; i++) WRITE_U64(MTIMECMP_ADDR(i), UINT64_MAX);
  riscv_fence();
  if(!failed) printf("MTIMECMP 64-bit/32-bit readback passed for all %d harts!\n", NUM_CORES);
  return failed;
}

void ipi_handler() {
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %lu: IPI raised!\n", id);
  clear_ipi(id);
  if(id == 0) {
    ipi_iter_cnt ++;
    riscv_fence();
    if(ipi_iter_cnt >= IPI_ITERATION) return;
  }
  raise_ipi((id + 1) % NUM_CORES);
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
  atomic_printf("Core %lu is started!\n", id);
  if(id == 0) {
    printf("IPI map: CLINT=0x%lx MSIP[0]=0x%lx MTIMECMP[0]=0x%lx PPU[0]=0x%lx timer=%lu Hz\n",
           CLINT_BASE_ADDR, MSIP_ADDR(0), MTIMECMP_ADDR(0), PPU_ADDR(0), TIMER_FREQ);
    if(probe_mtimecmp()) return 1;
    if(m_trap_handler_register(MSIP, ipi_handler)) return 1;
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