#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "spmc.h"
#include "ppu.h"

volatile uint8_t __attribute__((aligned(64))) global_buffer[NUM_CORES];

uint8_t test() {
  uint64_t iam = riscv_mhartid();
  uint8_t local_buffer[NUM_CORES];
  for(int i = 0; i < NUM_CORES; i++) local_buffer[i] = 0;
  for(int i = 0; i < 255; i++) {
    ++global_buffer[iam];
    riscv_fence();
    for(int j = 0; j < NUM_CORES; j++) {
      if(local_buffer[j] > global_buffer[j]) {
        atomic_printf("[ERROR]: Consumer %lu check failed, producer is %lu, local value is %d, get %d\n", iam, j, local_buffer[j], global_buffer[j]);
        return 1;
      }
      local_buffer[j] = global_buffer[j];
    }
  }
  return 0;
}

int main() {
  uint64_t iam = riscv_mhartid();
  uint8_t err;
  atomic_printf("[INFO]: hart %lu boot\n", iam);
  if(0 == iam) {
    for(int i = 1; i< NUM_CORES; i++) switch_on_core(i);
    for(int i = 0; i< NUM_CORES; i++) global_buffer[i] = 0;
    riscv_fence();
  }
  err = barrier(NUM_CORES);
  if(err) return err;
  
  for(int i = 0; i < ITERATION; i++) {
    if(test()) return 1;
    err = barrier(NUM_CORES);
    if(err) return err;
    global_buffer[iam] = 0;
    riscv_fence();
    if(iam == 0) atomic_printf("[INFO] :Iteration %d ends!\n", i);
    err = barrier(NUM_CORES);
    if(err) return err;
  }
  while(iam > 0) riscv_wfi();
  printf("[INFO]: SPMC test passed!\n");
  return 0;
}