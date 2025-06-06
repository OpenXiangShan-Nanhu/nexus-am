#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "poweron.h"
#include "ppu.h"

volatile uint8_t done_flags[NUM_CORES];

void hart_proc() {
  uint64_t iam = riscv_mhartid();
  done_flags[iam] = 1;
  riscv_fence();
  while(iam > 0) riscv_wfi();
}

int main() {
  uint64_t iam = riscv_mhartid();
  atomic_printf("hart %lu boot\n", iam);
  hart_proc();
  for(int i = 1; i < NUM_CORES ; i++) done_flags[i] = 0;
  done_flags[0] = 1;
  for(int i = 1; i< NUM_CORES; i++) switch_on_core(i);
  for(int i = 1; i< NUM_CORES; i++) while(done_flags[i] != 1);
  printf("all harts are enabled!\n");
  return 0;
}