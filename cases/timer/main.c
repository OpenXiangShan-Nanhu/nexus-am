#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "clint.h"
#include "csr.h"
#include "mtrap.h"
#include "platform.h"
#include <stdint.h>

#define INTR_PER_US 20
#define MAX_INTR 8

volatile uint64_t interval_cnt = 0;

void timer_intr_handler() {
  interval_cnt ++;
  printf("timer interrupt raised, %llu us passed!\n", interval_cnt * INTR_PER_US);
  uint64_t mtime = read_timer();
  write_cpu_mtimecmp(0, mtime + INTR_PER_US * MICROSECOND);
  riscv_fence();
}

int timer_intr_init() {
  printf("Initializing timer interrupt ...\n");

  if(m_trap_handler_register(MTIP, timer_intr_handler)) {
    return 1;
  }

  uint64_t mtime = read_timer();
  write_cpu_mtimecmp(0, mtime + INTR_PER_US * MICROSECOND);

  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MTIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
  return 0;
}

int main() {
  if(timer_intr_init()) {
    printf("Initialization failed!\n");
    return 1;
  }
  while(interval_cnt < (MAX_INTR - 1)) {
    riscv_wfi();
  }
  printf("Timer check successed!\n");
  return 0;
}