// Auto-generated instruction assembler
// Generated from JSON instruction definitions

#pragma once

#include <cstdint>

// Instruction implementations

template <long tcsr_id>
__attribute__((always_inline))
static inline void __tx_mat_tcsr_write(long rd) {
    constexpr long encoding = (((long)0b00000 << 27) | ((long)BaseNPUInstBuilder::TCSR_MAT << 23) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((tcsr_id & 1023) << 13);
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_write<encoding>(rd);
}

template <long tcsr_id>
__attribute__((always_inline))
static inline void __tx_mat_tcsr_read(long& rd) {
    constexpr long encoding = (((long)0b00000 << 27) | ((long)BaseNPUInstBuilder::TCSR_MAT << 23) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((tcsr_id & 1023) << 13);
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_read<encoding>(rd);
}

template <long tcsr_id>
__attribute__((always_inline))
static inline void __tx_vec_tcsr_write(long rd) {
    constexpr long encoding = (((long)0b00000 << 27) | ((long)BaseNPUInstBuilder::TCSR_VEC << 23) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((tcsr_id & 1023) << 13);
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_write<encoding>(rd);
}

template <long tcsr_id>
__attribute__((always_inline))
static inline void __tx_vec_tcsr_read(long& rd) {
    constexpr long encoding = (((long)0b00000 << 27) | ((long)BaseNPUInstBuilder::TCSR_VEC << 23) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((tcsr_id & 1023) << 13);
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_read<encoding>(rd);
}

template <long tcsr_id>
__attribute__((always_inline))
static inline void __tx_lsu_tcsr_write(long rd) {
    constexpr long encoding = (((long)0b00000 << 27) | ((long)BaseNPUInstBuilder::TCSR_LSU << 23) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((tcsr_id & 1023) << 13);
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_write<encoding>(rd);
}

template <long tcsr_id>
__attribute__((always_inline))
static inline void __tx_lsu_tcsr_read(long& rd) {
    constexpr long encoding = (((long)0b00000 << 27) | ((long)BaseNPUInstBuilder::TCSR_LSU << 23) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((tcsr_id & 1023) << 13);
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_read<encoding>(rd);
}

template <long tcsr_id>
__attribute__((always_inline))
static inline void __tx_macro_tcsr_write(long rd) {
    constexpr long encoding = (((long)0b00000 << 27) | ((long)BaseNPUInstBuilder::TCSR_MACRO << 23) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((tcsr_id & 1023) << 13);
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_write<encoding>(rd);
}

template <long tcsr_id>
__attribute__((always_inline))
static inline void __tx_macro_tcsr_read(long& rd) {
    constexpr long encoding = (((long)0b00000 << 27) | ((long)BaseNPUInstBuilder::TCSR_MACRO << 23) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((tcsr_id & 1023) << 13);
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_read<encoding>(rd);
}

__attribute__((always_inline))
static inline void __tx_mat_sb_produce(long rd) {
    constexpr long encoding = ((long)0b00000 << 27) | ((long)0b0000 << 23) | ((long)0x002 << 13) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B;
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_write<encoding>(rd);
}

__attribute__((always_inline))
static inline void __tx_mat_sb_bar(long& rd) {
    constexpr long encoding = ((long)0b00000 << 27) | ((long)0b0000 << 23) | ((long)0x002 << 13) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B;
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_read<encoding>(rd);
}

__attribute__((always_inline))
static inline void __tx_vec_sb_produce(long rd) {
    constexpr long encoding = ((long)0b00000 << 27) | ((long)0b0000 << 23) | ((long)0x003 << 13) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B;
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_write<encoding>(rd);
}

__attribute__((always_inline))
static inline void __tx_vec_sb_bar(long& rd) {
    constexpr long encoding = ((long)0b00000 << 27) | ((long)0b0000 << 23) | ((long)0x003 << 13) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B;
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_read<encoding>(rd);
}

__attribute__((always_inline))
static inline void __tx_lsu_sb_produce(long rd) {
    constexpr long encoding = ((long)0b00000 << 27) | ((long)0b0000 << 23) | ((long)0x001 << 13) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B;
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_write<encoding>(rd);
}

__attribute__((always_inline))
static inline void __tx_lsu_sb_bar(long& rd) {
    constexpr long encoding = ((long)0b00000 << 27) | ((long)0b0000 << 23) | ((long)0x001 << 13) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B;
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_read<encoding>(rd);
}

__attribute__((always_inline))
static inline void __tx_macro_sb_produce(long rd) {
    constexpr long encoding = ((long)0b00000 << 27) | ((long)0b0000 << 23) | ((long)0x004 << 13) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B;
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_write<encoding>(rd);
}

__attribute__((always_inline))
static inline void __tx_macro_sb_bar(long& rd) {
    constexpr long encoding = ((long)0b00000 << 27) | ((long)0b0000 << 23) | ((long)0x004 << 13) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B;
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_read<encoding>(rd);
}

__attribute__((always_inline))
static inline void __tx_sb_get_any(long& rd) {
    constexpr long encoding = ((long)0b00000 << 27) | ((long)0b0000 << 23) | ((long)0x000 << 13) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B;
    BaseNPUInstBuilder::__custom_32b_instruction_tcsr_sb_read<encoding>(rd);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_lsu_dma_u2u(long rd, long rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::LSU_FUNCT7 << 25) | (0b00 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((group & 7) << 20);
    BaseNPUInstBuilder::__custom_32b_instruction_lsu<encoding>(rd, rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_lsu_dma_u2d(long rd, long rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::LSU_FUNCT7 << 25) | (0b01 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((group & 7) << 20);
    BaseNPUInstBuilder::__custom_32b_instruction_lsu<encoding>(rd, rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_lsu_dma_d2u(long rd, long rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::LSU_FUNCT7 << 25) | (0b10 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((group & 7) << 20);
    BaseNPUInstBuilder::__custom_32b_instruction_lsu<encoding>(rd, rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_lsu_dma_d2d(long rd, long rs1) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::LSU_FUNCT7 << 25) | (0b11 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((group & 7) << 20);
    BaseNPUInstBuilder::__custom_32b_instruction_lsu<encoding>(rd, rs1);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_mat_mm(long rd, long rs1, long rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::MAT_FUNCT << 29) | ((long)0 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((group & 7) << 25);
    BaseNPUInstBuilder::__custom_32b_instruction_mat<encoding>(rd, rs1, rs2);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_mat_mma(long rd, long rs1, long rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::MAT_FUNCT << 29) | ((long)1 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((group & 7) << 25);
    BaseNPUInstBuilder::__custom_32b_instruction_mat<encoding>(rd, rs1, rs2);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_mat_conv2d(long rd, long rs1, long rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::MAT_FUNCT << 29) | ((long)2 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((group & 7) << 25);
    BaseNPUInstBuilder::__custom_32b_instruction_mat<encoding>(rd, rs1, rs2);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_mat_conv2da(long rd, long rs1, long rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::MAT_FUNCT << 29) | ((long)3 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((group & 7) << 25);
    BaseNPUInstBuilder::__custom_32b_instruction_mat<encoding>(rd, rs1, rs2);
}

template <long group>
__attribute__((always_inline))
static inline void __tx_mat_setptr(long rd, long rs1, long rs2) {
    constexpr long encoding = (((long)BaseNPUInstBuilder::MAT_FUNCT << 29) | ((long)4 << 12) | BaseNPUInstBuilder::OPCODE_CUSTOM_32B) | ((group & 7) << 25);
    BaseNPUInstBuilder::__custom_32b_instruction_mat<encoding>(rd, rs1, rs2);
}
