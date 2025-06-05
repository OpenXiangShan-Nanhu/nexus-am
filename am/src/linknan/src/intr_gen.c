#include "platform.h"
#include "intr_gen.h"

void raise_ext_intr(uint64_t id) {
  uint64_t bit_id = id - 1;
  uint64_t reg_id = bit_id / 64;
  uint64_t reg_bit = bit_id % 64;
  uint64_t reg_val = READ_U64(INTR_GEN_ADDR + 0x8 * reg_id);
  reg_bit = 1UL << reg_bit;
  reg_val |= reg_bit;
  WRITE_U64(INTR_GEN_ADDR + 0x8 * reg_id, reg_val);
}

void clear_ext_intr(uint64_t id) {
  uint64_t bit_id = id - 1;
  uint64_t reg_id = bit_id / 64;
  uint64_t reg_bit = bit_id % 64;
  uint64_t reg_val = READ_U64(INTR_GEN_ADDR + 0x8 * reg_id);
  reg_bit = 1UL << reg_bit;
  reg_val &= ~reg_bit;
  WRITE_U64(INTR_GEN_ADDR + 0x8 * reg_id, reg_val);
}