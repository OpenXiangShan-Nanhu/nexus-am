#ifndef __LINKNAN_PLIC_H__
#define __LINKNAN_PLIC_H__

#include <stdint.h>
#include "platform.h"

#define INTR_PRIO_REG(id)          (PLIC_BASE_ADDR + 0x4UL * (unsigned long)(id))
#define INTR_EN_REG(ctx, id)       (PLIC_BASE_ADDR + 0x002000UL + 0x80UL * (unsigned long)(ctx) + 0x4UL * (unsigned long)(id))
#define CTX_THD_REG(ctx)           (PLIC_BASE_ADDR + 0x200000UL + 0x1000UL * (unsigned long)(ctx))
#define CTX_COMP_REG(ctx)          (PLIC_BASE_ADDR + 0x200004UL + 0x1000UL * (unsigned long)(ctx))
#define MAX_PRIORITY               7

void plic_init(uint32_t core_num);

int setup_intr(uint32_t id, uint32_t priority);

int setup_context(uint32_t ctx, uint32_t threshold);

int enable_intr(uint32_t ctx, uint32_t id);

int disable_intr(uint32_t ctx, uint32_t id);

#endif
