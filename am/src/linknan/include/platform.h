#ifndef __LINKNAN_PLATFORM_H__
#define __LINKNAN_PLATFORM_H__

#include "stdint.h"

#define CPU_SPACE_BITS 18
#define NR_INTR        256

#define READ_U8(addr)        (*((volatile uint8_t *)(addr)))
#define WRITE_U8(addr, data) (*((volatile uint8_t *)(addr)) = (data))

#define READ_U16(addr)        (*((volatile uint16_t *)(addr)))
#define WRITE_U16(addr, data) (*((volatile uint16_t *)(addr)) = (data))

#define READ_U32(addr)        (*((volatile uint32_t *)(addr)))
#define WRITE_U32(addr, data) (*((volatile uint32_t *)(addr)) = (data))

#define READ_U64(addr)        (*((volatile uint64_t *)(addr)))
#define WRITE_U64(addr, data) (*((volatile uint64_t *)(addr)) = (data))

#define CPU_SPACE_BASE 0x01000000UL
#define CPU_SPACE(x) (CPU_SPACE_BASE + ((uintptr_t)(x) << CPU_SPACE_BITS))

#define BIT(n) (1UL << (n))
#define GENMASK(h, l) ((BIT((h)+1) - 1) & ~(BIT(l) - 1))
#define LSB(val) ((val) & -(val))

#define GET_FIELD(mask, value) ((value) & (mask) / LSB(mask))
#define SET_FIELD(mask, value) (((value) * LSB(mask)) & (mask))

#define BOOT_ADDR_OFFSET        0x0000UL
#define PPU_OFFSET              0x1000UL
#define TIMER_OFFSET            0x2000UL
#define MTIME_OFFSET            0x0000UL
#define MTIMECMP_OFFSET         0x0008UL
#define MSIP_OFFSET             0x0010UL
#define SSIP_OFFSET             0x0018UL

#define PLIC_BASE_ADDR          0x38050000UL
#define INTR_GEN_ADDR           0x40070000UL
#define TIMER_FREQ              10000000UL

#define L1D_SIZE 64 * 1024
#define L2C_SIZE 512 * 1024
#define L3C_SIZE 8 * 1024 * 1024

static inline void riscv_fence() {
  asm volatile("fence");
}

static inline void riscv_fence_i() {
  asm volatile("fence.i");
}

static inline void riscv_wfi() {
  asm volatile("wfi");
}

static inline uint64_t riscv_mhartid() {
  uint64_t result;
  asm volatile(
    "csrr %0, mhartid;"
    : "=r"(result)
  );
  return result;
}
#endif
