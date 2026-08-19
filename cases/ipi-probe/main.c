#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "clint.h"
#include "ppu.h"
#include "platform.h"
#include <stdint.h>

#define NUM_CORES 4

int main() {
  uint64_t id = riscv_mhartid();
  if (id == 0) {
    atomic_printf("Core 0 is started!\n");
    for (int i = 0; i < NUM_CORES; i++) switch_on_core(i);
  }
  if (barrier(NUM_CORES)) return 1;
  if (id == 0) {

    atomic_printf("Core 0: write mtimecmp(0) @ 0x%lx\n", MTIMECMP_ADDR(0));
    WRITE_U64(MTIMECMP_ADDR(0), 0xDEADBEEFCAFE0000UL);
    atomic_printf("Core 0: read mtimecmp(0)\n");
    uint64_t rd = READ_U64(MTIMECMP_ADDR(0));
    atomic_printf("Core 0: mtimecmp rd=0x%lx\n", rd);
    atomic_printf("Core 0: read mtime(0) @ 0x%lx\n", MTIME_ADDR(0));
    rd = READ_U64(MTIME_ADDR(0));
    atomic_printf("Core 0: mtime rd=0x%lx\n", rd);
    printf("PROBE PASS!\n");
  } else {
    while (1) riscv_wfi();
  }
  return 0;
}
