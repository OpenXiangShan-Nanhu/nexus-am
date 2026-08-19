#ifndef __LINKNAN_DACLINT_H__
#define __LINKNAN_DACLINT_H__

#include "platform.h"
#include "plic.h"

#define MTIME_ADDR(x)       (CPU_SPACE(x) + TIMER_OFFSET + MTIME_OFFSET)
#define MTIMECMP_ADDR(x)    (CPU_SPACE(x) + TIMER_OFFSET + MTIMECMP_OFFSET)

/* The ACLINT implements only the timer; IPIs are delivered as MSIs to the
 * target hart's IMSIC M interrupt file (identity IPI_EIID). */
#define IPI_EIID                     1UL
#define IMSIC_M_FILE_ADDR(x)         (CPU_SPACE(x) + IMSIC_OFFSET + IMSIC_M_FILE_OFFSET)
#define IMSIC_SETEIPNUM_ADDR(x)      (IMSIC_M_FILE_ADDR(x) + IMSIC_SETEIPNUM_OFFSET)
#define IMSIC_CLREIPNUM_ADDR(x)      (IMSIC_M_FILE_ADDR(x) + IMSIC_CLREIPNUM_OFFSET)

#define TIMER_FREQ              10000000UL

#define SECOND                  TIMER_FREQ
#define MILISECOND              (TIMER_FREQ / 1000UL)
#define MICROSECOND             (TIMER_FREQ / 1000000UL)

inline float ticks_to_ms(uint64_t timer_val) {
  const uint64_t div = MILISECOND;
  return (float)timer_val / (float)div;
}

inline float ticks_to_us(uint64_t timer_val) {
  const uint64_t div = MICROSECOND;
  return (float)timer_val / (float)div;
}

inline float ticks_to_s(uint64_t timer_val) {
  const uint64_t div = TIMER_FREQ;
  return (float)timer_val / (float)div;
}

inline void raise_ipi(int cpu) {
  WRITE_U32(IMSIC_SETEIPNUM_ADDR(cpu), IPI_EIID);
}

inline void clear_ipi(int cpu) {
  WRITE_U32(IMSIC_CLREIPNUM_ADDR(cpu), IPI_EIID);
}

/* Enable the local hart's IMSIC M file for IPI_EIID delivery (eie, eidelivery,
 * eithreshold) via the machine-level AIA CSRs. */
inline void imsic_ipi_enable(void) {
  imsic_enable_machine(IPI_EIID);
}

/* Claim (and clear) the top pending IMSIC M file interrupt; returns the
 * interrupt identity. */
inline uint32_t imsic_ipi_claim(void) {
  return imsic_claim_machine();
}

inline uint64_t read_timer() {
  uint64_t result;
  asm volatile ("rdtime %0" : "=r"(result));
  return result;
}

inline uint64_t read_cycle() {
  uint64_t result;
  asm volatile ("rdcycle %0" : "=r"(result));
  return result;
}

inline uint64_t read_cpu_mtimecmp(int cpu) {
  return READ_U64(MTIMECMP_ADDR(cpu));
}

inline void write_cpu_mtimecmp(int cpu, uint64_t mtimecmp) {
  WRITE_U64(MTIMECMP_ADDR(cpu), mtimecmp);
}

#endif