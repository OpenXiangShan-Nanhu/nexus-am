#ifndef _NPU_ALIAS_HPP_
#define _NPU_ALIAS_HPP_

#include "npu_kernel.h"
#include "gen_npu_instruction_64.h"

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_iv_add_fp32(void *rd, float rs1, void *rs2) {
    __tx_macro_elemwise_add_fp32<group, 2>(rd, (void*)(long) float_as_u32(rs1), rs2);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_iv_mul_fp32(void *rd, float rs1, void *rs2) {
    __tx_macro_elemwise_mul_fp32<group, 2>(rd, (void*)(long) float_as_u32(rs1), rs2);
}

static inline void __cflush_d_l1() {
    __asm__ __volatile__(".insn 0xfc000073" ::: "memory"); // CFLUSH.D.L1
}

#endif
