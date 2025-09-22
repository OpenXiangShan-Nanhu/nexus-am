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
#include "dw_wdt.h"

#define TEST_SIZE 5
#define WATCH_DOG_BASE 256  // 中断号256开始，对于 plic 输入线为 auto_in_255 开始
volatile uint64_t step = 0;


int main() {
  atomic_printf("The %d times boot\n", step);
  
  switch_power_mode(0, true, PWR_RET);

  step++;
  riscv_fence();
  riscv_cbo_flush((uint64_t)&step);

  dw_wdt_enable();

  if(step == TEST_SIZE){
    atomic_printf("CPU restart %d times\n", step);
    return 0;
  } else {
    riscv_wfi();  // 此处应该切换位RET
  }

}