// Auto-generated instruction assembler
// Generated from JSON instruction definitions

#pragma once

#include <cstdint>

// Instruction implementations

template <long group, long broadcast>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_add_s32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_ELEMENTWISE << 25ul) | ((long)BaseNPUInstBuilder::DT_S32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_ADD << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((broadcast & 3) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group, long broadcast>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_sub_s32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_ELEMENTWISE << 25ul) | ((long)BaseNPUInstBuilder::DT_S32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_SUB << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((broadcast & 3) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group, long broadcast>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_mul_s32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_ELEMENTWISE << 25ul) | ((long)BaseNPUInstBuilder::DT_S32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_MUL << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((broadcast & 3) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group, long broadcast>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_add_fp32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_ELEMENTWISE << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_ADD << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((broadcast & 3) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group, long broadcast>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_sub_fp32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_ELEMENTWISE << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_SUB << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((broadcast & 3) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group, long broadcast>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_mul_fp32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_ELEMENTWISE << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_MUL << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((broadcast & 3) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group, long broadcast>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_max_fp32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_ELEMENTWISE << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_MAX << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((broadcast & 3) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group, long broadcast>
__attribute__((always_inline))
static inline void __tx_macro_elemwise_min_fp32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_ELEMENTWISE << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_MIN << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((broadcast & 3) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_convert_fp32_s32_round(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_CONVERT << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::DT_S32 << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_convert_fp32_s32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_CONVERT << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::DT_S32 << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_convert_fp32_e4m3(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_CONVERT << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::DT_E4M3 << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_convert_fp32_fp16(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_CONVERT << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::DT_FP16 << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_convert_fp32_bf16(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_CONVERT << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::DT_BF16 << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group, long index>
__attribute__((always_inline))
static inline void __tx_macro_reduce_max_fp32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_REDUCE << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_MAX << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((index & 1) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group, long index>
__attribute__((always_inline))
static inline void __tx_macro_reduce_min_fp32(void *rd, void *rs1, void *rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_REDUCE << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_MIN << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((index & 1) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_3p<encoding>((long)rd, (long)rs1, (long)rs2);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_reduce_add_fp32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_REDUCE << 25ul) | ((long)BaseNPUInstBuilder::DT_FP32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_ADD << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group, long index>
__attribute__((always_inline))
static inline void __tx_macro_reduce_max_s32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_REDUCE << 25ul) | ((long)BaseNPUInstBuilder::DT_S32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_MAX << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((index & 1) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group, long index>
__attribute__((always_inline))
static inline void __tx_macro_reduce_min_s32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_REDUCE << 25ul) | ((long)BaseNPUInstBuilder::DT_S32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_MIN << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12) | ((index & 1) << 46);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_reduce_add_s32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_REDUCE << 25ul) | ((long)BaseNPUInstBuilder::DT_S32 << 32ul) | ((long)BaseNPUInstBuilder::TX_ELEM_ADD << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_sfu_log2_fp32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_SFU << 25ul) | ((long)BaseNPUInstBuilder::TX_SFU_LOG2 << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_sfu_exp2_fp32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_SFU << 25ul) | ((long)BaseNPUInstBuilder::TX_SFU_EXP2 << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_sfu_sin_fp32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_SFU << 25ul) | ((long)BaseNPUInstBuilder::TX_SFU_SIN << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_sfu_cos_fp32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_SFU << 25ul) | ((long)BaseNPUInstBuilder::TX_SFU_COS << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_sfu_rsqrt_fp32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_SFU << 25ul) | ((long)BaseNPUInstBuilder::TX_SFU_RSQRT << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_macro_sfu_recip_fp32(void *rd, void *rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::TX_EXT_SFU << 25ul) | ((long)BaseNPUInstBuilder::TX_SFU_RECIP << 40ul) | BaseNPUInstBuilder::OPCODE_CUSTOM_64B) | ((group & 7) << 12);
    BaseNPUInstBuilder::__custom_64b_macro_2p<encoding>((long)rd, (long)rs1);
}
