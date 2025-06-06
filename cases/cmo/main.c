#include <stdint.h>
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "platform.h"
#include "csr.h"
#include "cmo.h"
#include "clint.h"
#include "mtrap.h"

#define KiB 1024
#define MiB (1024 * 1024)
#define GiB (1024 * 1024 * 1024)

// #define TEST_RANGE (3 * 1024 * 1024)
#define TEST_RANGE (1 * KiB)
#define NR_ELEMENTS (TEST_RANGE / 8)
#define NR_CACHELINE (TEST_RANGE / 64)

volatile uint64_t __attribute__((aligned(64))) test_array[NR_ELEMENTS] = {0};

void invalid() {
  int64_t i = NR_CACHELINE;
  while(i --> 0) {
    riscv_cbo_inval((uint64_t)(&(test_array[i * 8])));
  }
}

void flush() {
  int64_t i = NR_CACHELINE;
  while(i --> 0) {
    riscv_cbo_flush((uint64_t)(&(test_array[i * 8])));
  }
}

void warm_cache(uint8_t stage) {
  printf("[INFO]: Cache warming up started!\n");
  const uint64_t flag_val = (uint64_t)stage << 55;
  int64_t i = NR_ELEMENTS;
  while(i --> 0) {
    test_array[i] = i | flag_val;
  }
  riscv_fence();
  printf("[INFO]: Cache warming up finished!\n");
}

void print_size(uint64_t size) {
  if(size >= GiB) {
    printf("%f GiB", (float)size / GiB);
  } else if(size >= MiB) {
    printf("%f MiB", (float)size / MiB);
  } else if(size >= KiB) {
    printf("%f KiB", (float)size / KiB);
  } else {
    printf("%lu B", size);
  }
}

void print_time(uint64_t time) {
  if(time >= SECOND) {
    printf("%f s\n", (float)time / SECOND);
  } else if(time >= MILISECOND) {
    printf("%f ms\n", (float)time / MILISECOND);
  } else if(time >= MICROSECOND) {
    printf("%f us\n", (float)time / MICROSECOND);
  } else {
    printf("%lu ticks\n", time);
  }
}

int main() {
  uint64_t start_time = 0;
  uint64_t end_time = 0;
  uint64_t elapsed_time = 0;

  warm_cache(1);
  start_time = read_timer();
  invalid();
  end_time = read_timer();
  elapsed_time = end_time - start_time;
  printf("[INFO]: ");
  print_time(TEST_RANGE);
  printf(" RO region flush takes ");
  print_time(elapsed_time);

  warm_cache(2);
  start_time = read_timer();
  flush();
  end_time = read_timer();
  elapsed_time = end_time - start_time;
  printf("[INFO]: ");
  print_time(TEST_RANGE);
  printf(" RW region flush takes ");
  print_time(elapsed_time);

  return 0;
}