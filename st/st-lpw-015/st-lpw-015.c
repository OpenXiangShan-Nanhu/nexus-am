#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "cmo.h"
#include "plic.h"
#include "mtrap.h"
#include "ppu.h"
#include "csr.h"
#include "platform.h"
#include "printf.h"
#include <stdint.h>


void enable_external_intr() {
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MEIE | MSIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
}

int main() {  
  enable_external_intr();
  switch_power_mode(0, true, PWR_RET);
  for(volatile int i = 0; i < 100000; i++);

  riscv_wfi();
  atomic_printf("Test finish\n");
}