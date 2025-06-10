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
  unsigned int pwr_plcy :2;
  unsigned int rsvd0    :6;
  unsigned int dyn_en   :1;
  unsigned int rsvd1    :23;
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


/**
 * Switches the specified core to ON power state
 * @param cpu hart id to operate on
 * @return 0 for accept
 */
int switch_on_core(int cpu);

/**
 * Switches the specified core to RETENEION power state
 * @param cpu hart id to operate on
 * @return 0 for accept, 1 for deny
 */

int switch_ret_core(int cpu);

/**
 * Switches the specified core to OFF power state
 * @param cpu hart id to operate on
 * @return 0 for accept, 1 for deny
 */
int switch_off_core(int cpu);


/**
 * Switches the specified core's power mode to static or dynamic
 * @param cpu hart id to operate on
 * @param policy lowest power level
 * @param mode true for dynamic, false for static
 */
void switch_power_mode(int cpu, int mode, int policy);
#endif