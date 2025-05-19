#ifndef __LINKNAN_DACLINT_H__
#define __LINKNAN_DACLINT_H__

#include "platform.h"

#define MTIME_ADDR(x)          (CPU_SPACE(x) + DACLINT_OFFSET + 0x0)
#define MTIMECMP_ADDR(x)       (CPU_SPACE(x) + DACLINT_OFFSET + 0x8)

#define TIMER_FREQ              1000000UL

#define SECOND                  TIMER_FREQ
#define MILISECOND              (TIMER_FREQ / 1000UL)
#define MICROSECOND             (TIMER_FREQ / 1000000UL)

inline float ticks_to_ms(uint64_t timer_val) {
  const uint64_t div = MILISECOND;
  return (float)timer_val / (float)div;
}

inline void raise_ipi(int cpu) {
  WRITE_U32(MSWI_BASE_ADDR + cpu * 0x4, 0x1);
}

inline void clear_ipi(int cpu) {
  WRITE_U32(MSWI_BASE_ADDR + cpu * 0x4, 0x0);
}

inline uint64_t read_timer() {
  uint64_t result;
  asm volatile(
    "csrr %0, mtime;"
    : "=r"(result)
  );
  return result;
}

inline uint64_t read_cpu_mtime(int cpu) {
  return READ_U64(MTIME_ADDR(cpu));
}

inline uint64_t read_cpu_mtimecmp(int cpu) {
  return READ_U64(MTIMECMP_ADDR(cpu));
}

#endif