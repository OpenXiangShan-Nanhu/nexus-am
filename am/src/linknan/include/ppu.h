#ifndef __LINKNAN_PPU_H__
#define __LINKNAN_PPU_H__

#include "platform.h"

#define PWPR_OFFSET 0x0
#define PWSR_OFFSET 0x4
#define IMR_OFFSET 0x8
#define IPR_OFFSET 0xc

#define PPU_ADDR(x) (CPU_SPACE(x) + PPU_OFFSET)
#define PWPR(x)     (PPU_ADDR(x)  + PWPR_OFFSET)
#define PWSR(x)     (PPU_ADDR(x)  + PWSR_OFFSET)
#define IMR(x)      (PPU_ADDR(x)  + IMR_OFFSET)
#define IPR(x)      (PPU_ADDR(x)  + IPR_OFFSET)

#define PWR_ON 2
#define PWR_RET 1
#define PWR_OFF 0

typedef struct {
  unsigned int pwr_plcy :8;
  unsigned int rsvd0    :8;
  unsigned int dyn_en   :8;
  unsigned int rsvd1    :8;
} PowerPolicy;

typedef struct {
  unsigned int dev      :8;
  unsigned int pcsm     :8;
  unsigned int rsvd     :16;
} PowerState;

typedef struct {
  unsigned int dyn_acpt :1;
  unsigned int dyn_deny :1;
  unsigned int stc_evnt :1;
  unsigned int rsvd     :29;
} IntrVec;

typedef union {
  uint32_t u32_val;
  PowerPolicy policy;
} PwprUnion;

typedef union {
  uint32_t u32_val;
  PowerState state;
} PwsrUnion;

typedef union {
  uint32_t u32_val;
  IntrVec intr;
} ImrUnion;

void switch_on_core(int core);

#endif