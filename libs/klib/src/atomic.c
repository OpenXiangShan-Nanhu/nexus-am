#include "klib.h"
#include <klib-macros.h>

uint32_t compare_and_swap(volatile uint32_t* addr, uint32_t old_val, uint32_t new_val) {
  uint32_t check = 0;
  uint32_t value = 0;
  __asm__ volatile (
    "lr.w %[value], (%[addr]);"
    : [value]"=r"(value)
    : [addr]"p"(addr)
  );
  if (value != old_val) return 1;
  __asm__ volatile (
    "sc.w %[check], %[write], (%[addr]);"
    : [check]"=r"(check)
    : [write]"r"(new_val), [addr]"p"(addr)
  );
  return check;
}

uint32_t atomic_add(volatile uint32_t *addr, uint32_t adder) {
  uint32_t result;
  __asm__ volatile(
    "amoadd.w %0, %1, (%2);"
    : "=r"(result)
    : "r"(adder), "r"(addr)
  );
  return result;
}

uint32_t atomic_swap(volatile uint32_t *addr, uint32_t swapper) {
  uint32_t result;
  __asm__ volatile(
    "amoswap.w %0, %1, (%2);"
    : "=r"(result)
    : "r"(swapper), "r"(addr)
  );
  return result;
}

volatile uint32_t _mstatus = 0;
void lock(volatile uint32_t *addr) {
  uint32_t mstatus;
  __asm__ volatile ("csrr %0, mstatus" : "=r" (mstatus));
  _mstatus = mstatus;
  __asm__ volatile("csrci mstatus, 0x8");
  while(compare_and_swap(addr, 0, 1));
}

void release(volatile uint32_t *addr) {
  *addr = 0;
  __asm__ volatile("fence");
  uint32_t mstatus = _mstatus;
  __asm__ volatile ("csrw mstatus, %0" : : "r" (mstatus));
}

uint8_t barrier(uint32_t threads) {
  static volatile uint32_t barrier_var = 0;
  static volatile uint32_t flipper = 0;
  uint32_t old_barrier_var;
  uint32_t iam;
  __asm__ volatile(
    "csrr %0, mhartid;"
    : "=r"(iam)
  );
  uint8_t main_thread = 0 == iam;

  if(main_thread) {
    atomic_swap(&flipper, 1);
    while((threads - 1) != barrier_var);
    old_barrier_var = atomic_swap(&barrier_var, 0);
    if(old_barrier_var >= threads) return 1;
    atomic_swap(&flipper, 0);
    while((threads - 1) != barrier_var);
    old_barrier_var = atomic_swap(&barrier_var, 0);
    if(old_barrier_var >= threads) return 1;
  } else {
    while(flipper == 0);
    old_barrier_var = atomic_add(&barrier_var, 1);
    if(old_barrier_var >= (threads - 1)) return 2;
    while(flipper == 1);
    old_barrier_var = atomic_add(&barrier_var, 1);
    if(old_barrier_var >= (threads - 1)) return 2;
  }
  return 0;
}

uint8_t s_barrier(uint32_t threads, uint32_t hartid) {
  static volatile uint32_t barrier_var = 0;
  static volatile uint32_t flipper = 0;
  uint32_t old_barrier_var;
  uint8_t main_thread = 0 == hartid;

  if(main_thread) {
    atomic_swap(&flipper, 1);
    while((threads - 1) != barrier_var);
    old_barrier_var = atomic_swap(&barrier_var, 0);
    if(old_barrier_var >= threads) return 1;
    atomic_swap(&flipper, 0);
    while((threads - 1) != barrier_var);
    old_barrier_var = atomic_swap(&barrier_var, 0);
    if(old_barrier_var >= threads) return 1;
  } else {
    while(flipper == 0);
    old_barrier_var = atomic_add(&barrier_var, 1);
    if(old_barrier_var >= (threads - 1)) return 2;
    while(flipper == 1);
    old_barrier_var = atomic_add(&barrier_var, 1);
    if(old_barrier_var >= (threads - 1)) return 2;
  }
  return 0;
}