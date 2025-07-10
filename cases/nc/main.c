#include <stdint.h>
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "platform.h"
#include "csr.h"
#include "clint.h"
#include "mtrap.h"
#include "xsextra.h"
#include "riscv.h"

#define KiB 1024
#define MiB (1024 * 1024)
#define GiB (1024 * 1024 * 1024)

#define PT_ENTRIES 512
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define TEST_RANGE (1 * KiB)
#define NR_ELEMENTS (TEST_RANGE / 8)
#define NR_CACHELINE (TEST_RANGE / 64)

#define NC_VA_BASE 0xc0000000
#define NC_PA_BASE 0xc0000000

#define EXCEPTION_LOAD_ACCESS_FAULT 5
#define EXCEPTION_STORE_ACCESS_FAULT 7
#define EXCEPTION_LOAD_PAGE_FAULT 13
#define EXCEPTION_STORE_PAGE_FAULT 15

void pbmt_nc_store(uint8_t stage) {
  printf("[INFO]: MMIO store started:\n");
  const uint64_t flag_val = (uint64_t)stage << 55;

  for (int64_t i = 0; i < NR_ELEMENTS; i++) {
    uint64_t addr = NC_VA_BASE + i * 8;
    uint64_t data = i | flag_val;
    WRITE_U64(addr, data);
  }

  printf("[INFO]: MMIO store finished!\n");
  return;
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

_Context *simple_trap(_Event ev, _Context *ctx) {
  switch(ev.event) {
    case _EVENT_IRQ_TIMER:
      printf("t"); break;
    case _EVENT_IRQ_IODEV:
      printf("d"); read_key(); break;
    case _EVENT_YIELD:
      printf("y"); break;
  }
  return ctx;
}

extern _AddressSpace kas;

static char *sv48_alloc_base = (char *)(0x84000000UL);
static uintptr_t sv48_alloced_size = 0;
void* sv48_pgalloc(size_t pg_size) {
  assert(pg_size == 0x1000);
  // printf("sv48 pgalloc called\n");
  void *ret = (void *)(sv48_alloc_base + sv48_alloced_size);
  sv48_alloced_size += pg_size;
  return ret;
}

void sv48_pgfree(void *ptr) {
return ;
}

int main() {
  asm volatile("csrw scounteren, %0" : : "r"(0xffffffffffffffff));
  asm volatile("csrw mcounteren, %0" : : "r"(0xffffffffffffffff));
  uint64_t menvcfgVal;
  asm volatile("csrr %0, 0x30a" : "=r"(menvcfgVal));
  menvcfgVal |= (1ULL << 62);
  asm volatile("csrw 0x30a, %0" : : "r"(menvcfgVal));
  _cte_init(simple_trap);
  _vme_init_with_test(sv48_pgalloc, sv48_pgfree);

  riscv_fence();
  printf("[INFO]: FENCE beforce PMT NC store test\n");
  uint64_t start_time = 0;
  uint64_t end_time = 0;
  uint64_t elapsed_time = 0;
  start_time = read_cycle();
  pbmt_nc_store(1);
  riscv_fence();
  end_time = read_cycle();
  elapsed_time = end_time - start_time;
  printf("[INFO]: MMIO store took ");
  printf("%lu cycles\n", elapsed_time);
  printf("\n");

  return 0;
}
