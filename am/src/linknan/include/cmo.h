#ifndef __LINKNAN_CMO_H__
#define __LINKNAN_CMO_H__

#include "stdint.h"

#define CACHELINE_SIZE 64

inline void riscv_cbo_clean(uint64_t addr) {
  __asm__ volatile(
    "cbo.clean (%0)"
    :
    : "r"(addr)
    : "memory"
  );
}

inline void riscv_cbo_flush(uint64_t addr) {
  __asm__ volatile(
    "cbo.flush (%0)"
    :
    : "r"(addr)
    : "memory"
  );
}

inline void riscv_cbo_inval(uint64_t addr) {
  __asm__ volatile(
    "cbo.inval (%0)"
    :
    : "r"(addr)
    : "memory"
  );
}

void mem_invalid(const volatile uint8_t *array, uint64_t size);

void mem_flush(const volatile uint8_t *array, uint64_t size);

#endif