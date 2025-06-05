#include "platform.h"
#include "plic.h"

void plic_init(uint32_t core_num) {
  for(int i = 1; i <= NR_INTR; i ++) {
    WRITE_U32(INTR_PRIO_REG(i), 0);
  }
  for(int i = 0; i < core_num * 2; i ++) {
    for(int j = 0; j < (NR_INTR + 31) / 32; j ++) {
      WRITE_U32(INTR_EN_REG(i, j), 0);
    }
    WRITE_U32(CTX_THD_REG(i), 0);
  }
}

int setup_intr(uint32_t id, uint32_t priority) {
  if(priority > MAX_PRIORITY) {
    return 1;
  }
  WRITE_U32(INTR_PRIO_REG(id), priority);
  return 0;
}

int setup_context(uint32_t ctx, uint32_t threshold) {
  if(threshold > MAX_PRIORITY) {
    return 1;
  }
  WRITE_U32(CTX_THD_REG(ctx), threshold);
  return 0;
}

int enable_intr(uint32_t ctx, uint32_t id) {
  if(id > NR_INTR || id == 0) {
    return 1;
  }
  uint32_t en_reg_id = id / 32;
  uint32_t en_reg_bit = id % 32;
  uint32_t en_val = READ_U32(INTR_EN_REG(ctx, en_reg_id));
  en_reg_bit = 1 << en_reg_bit;
  en_val |= en_reg_bit;
  WRITE_U32(INTR_EN_REG(ctx, en_reg_id), en_val);
  return 0;
}

int disable_intr(uint32_t ctx, uint32_t id) {
  if(id > NR_INTR || id == 0) {
    return 1;
  }
  uint32_t en_reg_id = id / 32;
  uint32_t en_reg_bit = id % 32;
  uint32_t en_val = READ_U32(INTR_EN_REG(ctx, en_reg_id));
  en_reg_bit = 1 << en_reg_bit;
  en_val &= ~en_reg_bit;
  WRITE_U32(INTR_EN_REG(ctx, en_reg_id), en_val);
  return 0;
}