#include "cmo.h"

void mem_invalid(const volatile uint8_t *array, uint64_t size) {
  int64_t i = (size + CACHELINE_SIZE - 1) / CACHELINE_SIZE;
  while(i --> 0) {
    riscv_cbo_inval((uint64_t)(&(array[i * CACHELINE_SIZE])));
  }
  riscv_fence(); // Nanhu-V5 needs this to ensure all cmo are done
}

void mem_flush(const volatile uint8_t *array, uint64_t size) {
  int64_t i = (size + CACHELINE_SIZE - 1) / CACHELINE_SIZE;
  while(i --> 0) {
    riscv_cbo_flush((uint64_t)(&(array[i * CACHELINE_SIZE])));
  }
  riscv_fence(); // Nanhu-V5 needs this to ensure all cmo are done
}