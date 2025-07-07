#include <printf.h>
#include <stdint.h>
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "platform.h"
#include "ppu.h"

#define NUM_CORES 4
#define BUF_SIZE (L2C_SIZE + 64 * 1024)
#define BUF_ELMTS (BUF_SIZE / sizeof(uint64_t))

volatile uint64_t buffer[NUM_CORES][BUF_ELMTS];

void warmup(volatile uint64_t *buf) {
  for(size_t i = 0; i < BUF_ELMTS; i ++) buf[i] = i;
}

// Read and write cachelines missed in L2 but hit in L3, to trigger burst writes and a reads to LLC
#define CHKB_0 0x5555555555555555
#define CHKB_1 0xAAAAAAAAAAAAAAAA
void test(volatile uint64_t *buf) {
  for(size_t i = 0; i < (BUF_SIZE - L2C_SIZE) / sizeof(uint64_t); i ++) {
    buf[i] = (i & (0x1 << 5)) == 0? CHKB_0: CHKB_1;
  }
}

int main() {
  uint64_t id = riscv_mhartid();
  atomic_printf("Core %lu is started!\n", id);
  if(id == 0) {
    for(int i = 1; i < NUM_CORES; i++) switch_on_core(i);
  }
  warmup(buffer[id]);
  if(barrier(NUM_CORES)) return 1;
  if(id == 0) {
    printf("Warmup done, start testing ...\n");
  }
  if(barrier(NUM_CORES)) return 1;
  test(buffer[id]);
  if(id == 0) {
    printf("Test done\n");
  } else {
    while(1) riscv_wfi();
  }
}