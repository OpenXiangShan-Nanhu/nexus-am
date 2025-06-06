#ifndef __LINKNAN_CMO_H__
#define __LINKNAN_CMO_H__

#include "stdint.h"

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
#endif