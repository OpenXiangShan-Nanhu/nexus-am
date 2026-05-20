#ifndef _NPU_INSTRUCTION_HPP_
#define _NPU_INSTRUCTION_HPP_

#include <stdint.h>

typedef unsigned long ulong;

#define _inst_builder_t template <long encoding> __attribute__((always_inline)) static inline void

namespace BaseNPUInstBuilder {

// ====== 指令常量 ======

// 操作码 (7 bits at [6:0])
constexpr long OPCODE_CUSTOM_64B = 0x3F;  // 0b0111111
constexpr long OPCODE_CUSTOM_32B = 0x0B;  // 0b0001011

// ====== 32位指令常量 ======

// TCSR 单元编码 (5 bits at [25:23])
constexpr long TCSR_LSU = 0x01;  // 0b0001
constexpr long TCSR_MAT = 0x02;  // 0b0010
constexpr long TCSR_VEC = 0x03;  // 0b0011
constexpr long TCSR_MACRO = 0x04;  // 0b0100

// MAT 指令类型编码 (3 bits at [31:29])
constexpr long MAT_FUNCT = 0x01;  // 0b001 - 矩阵乘

// LSU LSU funct7 编码
constexpr long LSU_FUNCT7 = 0x04;  // 0b0000100

// ====== 64位指令常量 ======

// 扩展编码
constexpr long TX_EXT_ELEMENTWISE = 0x00;  // 0b0000000 - 元素运算
constexpr long TX_EXT_CONVERT = 0x01;       // 0b0000001 - 类型转换
constexpr long TX_EXT_SFU = 0x02;          // 0b0000010 - 特殊函数单元
constexpr long TX_EXT_REDUCE = 0x03;        // 0b0000011 - 规约操作

// 数据类型编码
constexpr long DT_FP32 = 0x00;      // 0b00000000 - FP32
constexpr long DT_BF16 = 0x02;      // 0b00000010 - BF16
constexpr long DT_FP16 = 0x03;      // 0b00000011 - FP16
constexpr long DT_E4M3 = 0x04;     // 0b00000100 - FP8 E4M3
constexpr long DT_E5M2 = 0x05;     // 0b00000101 - FP8 E5M2
constexpr long DT_MXFP8_E4M3 = 0x06;  // 0b00000110 - MXFP8 E4M3
constexpr long DT_MXFP8_E5M2 = 0x07;  // 0b00000111 - MXFP8 E5M2
constexpr long DT_S32 = 0x20;       // 0b00100000 - 有符号32位
constexpr long DT_U32 = 0x21;       // 0b00100001 - 无符号32位
constexpr long DT_S16 = 0x22;       // 0b00100010 - 有符号16位
constexpr long DT_U16 = 0x23;       // 0b00100011 - 无符号16位
constexpr long DT_S8 = 0x24;        // 0b00100100 - 有符号8位
constexpr long DT_U8 = 0x25;        // 0b00100101 - 无符号8位

// 元素运算操作编码
constexpr long TX_ELEM_ADD = 0x00;  // 0b000000
constexpr long TX_ELEM_SUB = 0x01;  // 0b000001
constexpr long TX_ELEM_MUL = 0x02;  // 0b000010
constexpr long TX_ELEM_MAX = 0x04;  // 0b000100 - 浮点最大值
constexpr long TX_ELEM_MIN = 0x05;  // 0b000101 - 浮点最小值

// SFU 操作编码
constexpr long TX_SFU_LOG2 = 0;   // 0b00000000 - ReLU
constexpr long TX_SFU_EXP2 = 1;   // 0b00000001 - 指数
constexpr long TX_SFU_SIN  = 2;   // 0b00000010 - 正弦
constexpr long TX_SFU_COS  = 3;   // 0b00000011 - 余弦
constexpr long TX_SFU_RSQRT = 4;  // 0b00000100 - 平方根倒数
constexpr long TX_SFU_RECIP = 5;  // 0b00000101 - 倒数

// ====== 常量


_inst_builder_t __custom_64b_macro_3p(long rd, long rs1, long rs2) {
    asm volatile(
        "\n"
        ".ifc %0,zero\n.set _R0,0\n.endif\n.ifc %0,ra\n.set _R0,1\n.endif\n.ifc %0,sp\n.set _R0,2\n.endif\n.ifc %0,gp\n.set _R0,3\n.endif\n.ifc %0,tp\n.set _R0,4\n.endif\n.ifc %0,t0\n.set _R0,5\n.endif\n.ifc %0,t1\n.set _R0,6\n.endif\n.ifc %0,t2\n.set _R0,7\n.endif\n.ifc %0,s0\n.set _R0,8\n.endif\n.ifc %0,s1\n.set _R0,9\n.endif\n.ifc %0,a0\n.set _R0,10\n.endif\n.ifc %0,a1\n.set _R0,11\n.endif\n.ifc %0,a2\n.set _R0,12\n.endif\n.ifc %0,a3\n.set _R0,13\n.endif\n.ifc %0,a4\n.set _R0,14\n.endif\n.ifc %0,a5\n.set _R0,15\n.endif\n.ifc %0,a6\n.set _R0,16\n.endif\n.ifc %0,a7\n.set _R0,17\n.endif\n.ifc %0,s2\n.set _R0,18\n.endif\n.ifc %0,s3\n.set _R0,19\n.endif\n.ifc %0,s4\n.set _R0,20\n.endif\n.ifc %0,s5\n.set _R0,21\n.endif\n.ifc %0,s6\n.set _R0,22\n.endif\n.ifc %0,s7\n.set _R0,23\n.endif\n.ifc %0,s8\n.set _R0,24\n.endif\n.ifc %0,s9\n.set _R0,25\n.endif\n.ifc %0,s10\n.set _R0,26\n.endif\n.ifc %0,s11\n.set _R0,27\n.endif\n.ifc %0,t3\n.set _R0,28\n.endif\n.ifc %0,t4\n.set _R0,29\n.endif\n.ifc %0,t5\n.set _R0,30\n.endif\n.ifc %0,t6\n.set _R0,31\n.endif\n.ifc %1,zero\n.set _R1,0\n.endif\n.ifc %1,ra\n.set _R1,1\n.endif\n.ifc %1,sp\n.set _R1,2\n.endif\n.ifc %1,gp\n.set _R1,3\n.endif\n.ifc %1,tp\n.set _R1,4\n.endif\n.ifc %1,t0\n.set _R1,5\n.endif\n.ifc %1,t1\n.set _R1,6\n.endif\n.ifc %1,t2\n.set _R1,7\n.endif\n.ifc %1,s0\n.set _R1,8\n.endif\n.ifc %1,s1\n.set _R1,9\n.endif\n.ifc %1,a0\n.set _R1,10\n.endif\n.ifc %1,a1\n.set _R1,11\n.endif\n.ifc %1,a2\n.set _R1,12\n.endif\n.ifc %1,a3\n.set _R1,13\n.endif\n.ifc %1,a4\n.set _R1,14\n.endif\n.ifc %1,a5\n.set _R1,15\n.endif\n.ifc %1,a6\n.set _R1,16\n.endif\n.ifc %1,a7\n.set _R1,17\n.endif\n.ifc %1,s2\n.set _R1,18\n.endif\n.ifc %1,s3\n.set _R1,19\n.endif\n.ifc %1,s4\n.set _R1,20\n.endif\n.ifc %1,s5\n.set _R1,21\n.endif\n.ifc %1,s6\n.set _R1,22\n.endif\n.ifc %1,s7\n.set _R1,23\n.endif\n.ifc %1,s8\n.set _R1,24\n.endif\n.ifc %1,s9\n.set _R1,25\n.endif\n.ifc %1,s10\n.set _R1,26\n.endif\n.ifc %1,s11\n.set _R1,27\n.endif\n.ifc %1,t3\n.set _R1,28\n.endif\n.ifc %1,t4\n.set _R1,29\n.endif\n.ifc %1,t5\n.set _R1,30\n.endif\n.ifc %1,t6\n.set _R1,31\n.endif\n.ifc %2,zero\n.set _R2,0\n.endif\n.ifc %2,ra\n.set _R2,1\n.endif\n.ifc %2,sp\n.set _R2,2\n.endif\n.ifc %2,gp\n.set _R2,3\n.endif\n.ifc %2,tp\n.set _R2,4\n.endif\n.ifc %2,t0\n.set _R2,5\n.endif\n.ifc %2,t1\n.set _R2,6\n.endif\n.ifc %2,t2\n.set _R2,7\n.endif\n.ifc %2,s0\n.set _R2,8\n.endif\n.ifc %2,s1\n.set _R2,9\n.endif\n.ifc %2,a0\n.set _R2,10\n.endif\n.ifc %2,a1\n.set _R2,11\n.endif\n.ifc %2,a2\n.set _R2,12\n.endif\n.ifc %2,a3\n.set _R2,13\n.endif\n.ifc %2,a4\n.set _R2,14\n.endif\n.ifc %2,a5\n.set _R2,15\n.endif\n.ifc %2,a6\n.set _R2,16\n.endif\n.ifc %2,a7\n.set _R2,17\n.endif\n.ifc %2,s2\n.set _R2,18\n.endif\n.ifc %2,s3\n.set _R2,19\n.endif\n.ifc %2,s4\n.set _R2,20\n.endif\n.ifc %2,s5\n.set _R2,21\n.endif\n.ifc %2,s6\n.set _R2,22\n.endif\n.ifc %2,s7\n.set _R2,23\n.endif\n.ifc %2,s8\n.set _R2,24\n.endif\n.ifc %2,s9\n.set _R2,25\n.endif\n.ifc %2,s10\n.set _R2,26\n.endif\n.ifc %2,s11\n.set _R2,27\n.endif\n.ifc %2,t3\n.set _R2,28\n.endif\n.ifc %2,t4\n.set _R2,29\n.endif\n.ifc %2,t5\n.set _R2,30\n.endif\n.ifc %2,t6\n.set _R2,31\n.endif\n"
        ".align 3\n"
        ".insn 8, %3 | 0x3F | (_R2 << 7) | (_R0 << 15) | (_R1 << 20)\n"
        ".set _R0,z\n.set _R1,z\n.set _R2,z\n" ::"r"(rs1), "r"(rs2), "r"(rd), "i"(encoding));
}

_inst_builder_t __custom_64b_macro_2p(long rd, long rs1) {
    asm volatile(
        "\n"
        ".ifc %0,zero\n.set _R0,0\n.endif\n.ifc %0,ra\n.set _R0,1\n.endif\n.ifc %0,sp\n.set _R0,2\n.endif\n.ifc %0,gp\n.set _R0,3\n.endif\n.ifc %0,tp\n.set _R0,4\n.endif\n.ifc %0,t0\n.set _R0,5\n.endif\n.ifc %0,t1\n.set _R0,6\n.endif\n.ifc %0,t2\n.set _R0,7\n.endif\n.ifc %0,s0\n.set _R0,8\n.endif\n.ifc %0,s1\n.set _R0,9\n.endif\n.ifc %0,a0\n.set _R0,10\n.endif\n.ifc %0,a1\n.set _R0,11\n.endif\n.ifc %0,a2\n.set _R0,12\n.endif\n.ifc %0,a3\n.set _R0,13\n.endif\n.ifc %0,a4\n.set _R0,14\n.endif\n.ifc %0,a5\n.set _R0,15\n.endif\n.ifc %0,a6\n.set _R0,16\n.endif\n.ifc %0,a7\n.set _R0,17\n.endif\n.ifc %0,s2\n.set _R0,18\n.endif\n.ifc %0,s3\n.set _R0,19\n.endif\n.ifc %0,s4\n.set _R0,20\n.endif\n.ifc %0,s5\n.set _R0,21\n.endif\n.ifc %0,s6\n.set _R0,22\n.endif\n.ifc %0,s7\n.set _R0,23\n.endif\n.ifc %0,s8\n.set _R0,24\n.endif\n.ifc %0,s9\n.set _R0,25\n.endif\n.ifc %0,s10\n.set _R0,26\n.endif\n.ifc %0,s11\n.set _R0,27\n.endif\n.ifc %0,t3\n.set _R0,28\n.endif\n.ifc %0,t4\n.set _R0,29\n.endif\n.ifc %0,t5\n.set _R0,30\n.endif\n.ifc %0,t6\n.set _R0,31\n.endif\n.ifc %1,zero\n.set _R1,0\n.endif\n.ifc %1,ra\n.set _R1,1\n.endif\n.ifc %1,sp\n.set _R1,2\n.endif\n.ifc %1,gp\n.set _R1,3\n.endif\n.ifc %1,tp\n.set _R1,4\n.endif\n.ifc %1,t0\n.set _R1,5\n.endif\n.ifc %1,t1\n.set _R1,6\n.endif\n.ifc %1,t2\n.set _R1,7\n.endif\n.ifc %1,s0\n.set _R1,8\n.endif\n.ifc %1,s1\n.set _R1,9\n.endif\n.ifc %1,a0\n.set _R1,10\n.endif\n.ifc %1,a1\n.set _R1,11\n.endif\n.ifc %1,a2\n.set _R1,12\n.endif\n.ifc %1,a3\n.set _R1,13\n.endif\n.ifc %1,a4\n.set _R1,14\n.endif\n.ifc %1,a5\n.set _R1,15\n.endif\n.ifc %1,a6\n.set _R1,16\n.endif\n.ifc %1,a7\n.set _R1,17\n.endif\n.ifc %1,s2\n.set _R1,18\n.endif\n.ifc %1,s3\n.set _R1,19\n.endif\n.ifc %1,s4\n.set _R1,20\n.endif\n.ifc %1,s5\n.set _R1,21\n.endif\n.ifc %1,s6\n.set _R1,22\n.endif\n.ifc %1,s7\n.set _R1,23\n.endif\n.ifc %1,s8\n.set _R1,24\n.endif\n.ifc %1,s9\n.set _R1,25\n.endif\n.ifc %1,s10\n.set _R1,26\n.endif\n.ifc %1,s11\n.set _R1,27\n.endif\n.ifc %1,t3\n.set _R1,28\n.endif\n.ifc %1,t4\n.set _R1,29\n.endif\n.ifc %1,t5\n.set _R1,30\n.endif\n.ifc %1,t6\n.set _R1,31\n.endif\n"
        ".align 3\n"
        ".insn 8, %2 | 0x3F | (_R1 << 7) | (_R0 << 15)\n"
        ".set _R0,z\n.set _R1,z\n" ::"r"(rs1), "r"(rd), "i"(encoding));
}

_inst_builder_t __custom_32b_instruction_mat(long rd, long rs1, long rs2) {
    asm volatile(
        "\n"
        ".ifc %0,zero\n.set _R0,0\n.endif\n.ifc %0,ra\n.set _R0,1\n.endif\n.ifc %0,sp\n.set "
        "_R0,2\n.endif\n.ifc %0,gp\n.set _R0,3\n.endif\n.ifc %0,tp\n.set _R0,4\n.endif\n.ifc "
        "%0,t0\n.set _R0,5\n.endif\n.ifc %0,t1\n.set _R0,6\n.endif\n.ifc %0,t2\n.set "
        "_R0,7\n.endif\n.ifc %0,s0\n.set _R0,8\n.endif\n.ifc %0,s1\n.set _R0,9\n.endif\n.ifc "
        "%0,a0\n.set _R0,10\n.endif\n.ifc %0,a1\n.set _R0,11\n.endif\n.ifc %0,a2\n.set "
        "_R0,12\n.endif\n.ifc %0,a3\n.set _R0,13\n.endif\n.ifc %0,a4\n.set "
        "_R0,14\n.endif\n.ifc %0,a5\n.set _R0,15\n.endif\n.ifc %0,a6\n.set "
        "_R0,16\n.endif\n.ifc %0,a7\n.set _R0,17\n.endif\n.ifc %0,s2\n.set "
        "_R0,18\n.endif\n.ifc %0,s3\n.set _R0,19\n.endif\n.ifc %0,s4\n.set "
        "_R0,20\n.endif\n.ifc %0,s5\n.set _R0,21\n.endif\n.ifc %0,s6\n.set "
        "_R0,22\n.endif\n.ifc %0,s7\n.set _R0,23\n.endif\n.ifc %0,s8\n.set "
        "_R0,24\n.endif\n.ifc %0,s9\n.set _R0,25\n.endif\n.ifc %0,s10\n.set "
        "_R0,26\n.endif\n.ifc %0,s11\n.set _R0,27\n.endif\n.ifc %0,t3\n.set "
        "_R0,28\n.endif\n.ifc %0,t4\n.set _R0,29\n.endif\n.ifc %0,t5\n.set "
        "_R0,30\n.endif\n.ifc %0,t6\n.set _R0,31\n.endif\n.ifc %1,zero\n.set "
        "_R1,0\n.endif\n.ifc %1,ra\n.set _R1,1\n.endif\n.ifc %1,sp\n.set _R1,2\n.endif\n.ifc "
        "%1,gp\n.set _R1,3\n.endif\n.ifc %1,tp\n.set _R1,4\n.endif\n.ifc %1,t0\n.set "
        "_R1,5\n.endif\n.ifc %1,t1\n.set _R1,6\n.endif\n.ifc %1,t2\n.set _R1,7\n.endif\n.ifc "
        "%1,s0\n.set _R1,8\n.endif\n.ifc %1,s1\n.set _R1,9\n.endif\n.ifc %1,a0\n.set "
        "_R1,10\n.endif\n.ifc %1,a1\n.set _R1,11\n.endif\n.ifc %1,a2\n.set "
        "_R1,12\n.endif\n.ifc %1,a3\n.set _R1,13\n.endif\n.ifc %1,a4\n.set "
        "_R1,14\n.endif\n.ifc %1,a5\n.set _R1,15\n.endif\n.ifc %1,a6\n.set "
        "_R1,16\n.endif\n.ifc %1,a7\n.set _R1,17\n.endif\n.ifc %1,s2\n.set "
        "_R1,18\n.endif\n.ifc %1,s3\n.set _R1,19\n.endif\n.ifc %1,s4\n.set "
        "_R1,20\n.endif\n.ifc %1,s5\n.set _R1,21\n.endif\n.ifc %1,s6\n.set "
        "_R1,22\n.endif\n.ifc %1,s7\n.set _R1,23\n.endif\n.ifc %1,s8\n.set "
        "_R1,24\n.endif\n.ifc %1,s9\n.set _R1,25\n.endif\n.ifc %1,s10\n.set "
        "_R1,26\n.endif\n.ifc %1,s11\n.set _R1,27\n.endif\n.ifc %1,t3\n.set "
        "_R1,28\n.endif\n.ifc %1,t4\n.set _R1,29\n.endif\n.ifc %1,t5\n.set "
        "_R1,30\n.endif\n.ifc %1,t6\n.set _R1,31\n.endif\n.ifc %2,zero\n.set "
        "_R2,0\n.endif\n.ifc %2,ra\n.set _R2,1\n.endif\n.ifc %2,sp\n.set _R2,2\n.endif\n.ifc "
        "%2,gp\n.set _R2,3\n.endif\n.ifc %2,tp\n.set _R2,4\n.endif\n.ifc %2,t0\n.set "
        "_R2,5\n.endif\n.ifc %2,t1\n.set _R2,6\n.endif\n.ifc %2,t2\n.set _R2,7\n.endif\n.ifc "
        "%2,s0\n.set _R2,8\n.endif\n.ifc %2,s1\n.set _R2,9\n.endif\n.ifc %2,a0\n.set "
        "_R2,10\n.endif\n.ifc %2,a1\n.set _R2,11\n.endif\n.ifc %2,a2\n.set "
        "_R2,12\n.endif\n.ifc %2,a3\n.set _R2,13\n.endif\n.ifc %2,a4\n.set "
        "_R2,14\n.endif\n.ifc %2,a5\n.set _R2,15\n.endif\n.ifc %2,a6\n.set "
        "_R2,16\n.endif\n.ifc %2,a7\n.set _R2,17\n.endif\n.ifc %2,s2\n.set "
        "_R2,18\n.endif\n.ifc %2,s3\n.set _R2,19\n.endif\n.ifc %2,s4\n.set "
        "_R2,20\n.endif\n.ifc %2,s5\n.set _R2,21\n.endif\n.ifc %2,s6\n.set "
        "_R2,22\n.endif\n.ifc %2,s7\n.set _R2,23\n.endif\n.ifc %2,s8\n.set "
        "_R2,24\n.endif\n.ifc %2,s9\n.set _R2,25\n.endif\n.ifc %2,s10\n.set "
        "_R2,26\n.endif\n.ifc %2,s11\n.set _R2,27\n.endif\n.ifc %2,t3\n.set "
        "_R2,28\n.endif\n.ifc %2,t4\n.set _R2,29\n.endif\n.ifc %2,t5\n.set "
        "_R2,30\n.endif\n.ifc %2,t6\n.set _R2,31\n.endif\n"
        ".align 2\n"
        ".insn 4, %3 | (_R0 << 7) | (_R1 << 15) | (_R2 << 20)\n"
        ".set _R0,z\n.set _R1,z\n.set _R2,z\n" ::"r"(rd),
        "r"(rs1), "r"(rs2), "i"(encoding));
}
_inst_builder_t __custom_32b_instruction_lsu(long rd, long rs1) {
    asm volatile(
        "\n"
        ".ifc %0,zero\n.set _R0,0\n.endif\n.ifc %0,ra\n.set _R0,1\n.endif\n.ifc %0,sp\n.set "
        "_R0,2\n.endif\n.ifc %0,gp\n.set _R0,3\n.endif\n.ifc %0,tp\n.set _R0,4\n.endif\n.ifc "
        "%0,t0\n.set _R0,5\n.endif\n.ifc %0,t1\n.set _R0,6\n.endif\n.ifc %0,t2\n.set "
        "_R0,7\n.endif\n.ifc %0,s0\n.set _R0,8\n.endif\n.ifc %0,s1\n.set _R0,9\n.endif\n.ifc "
        "%0,a0\n.set _R0,10\n.endif\n.ifc %0,a1\n.set _R0,11\n.endif\n.ifc %0,a2\n.set "
        "_R0,12\n.endif\n.ifc %0,a3\n.set _R0,13\n.endif\n.ifc %0,a4\n.set "
        "_R0,14\n.endif\n.ifc %0,a5\n.set _R0,15\n.endif\n.ifc %0,a6\n.set "
        "_R0,16\n.endif\n.ifc %0,a7\n.set _R0,17\n.endif\n.ifc %0,s2\n.set "
        "_R0,18\n.endif\n.ifc %0,s3\n.set _R0,19\n.endif\n.ifc %0,s4\n.set "
        "_R0,20\n.endif\n.ifc %0,s5\n.set _R0,21\n.endif\n.ifc %0,s6\n.set "
        "_R0,22\n.endif\n.ifc %0,s7\n.set _R0,23\n.endif\n.ifc %0,s8\n.set "
        "_R0,24\n.endif\n.ifc %0,s9\n.set _R0,25\n.endif\n.ifc %0,s10\n.set "
        "_R0,26\n.endif\n.ifc %0,s11\n.set _R0,27\n.endif\n.ifc %0,t3\n.set "
        "_R0,28\n.endif\n.ifc %0,t4\n.set _R0,29\n.endif\n.ifc %0,t5\n.set "
        "_R0,30\n.endif\n.ifc %0,t6\n.set _R0,31\n.endif\n.ifc %1,zero\n.set "
        "_R1,0\n.endif\n.ifc %1,ra\n.set _R1,1\n.endif\n.ifc %1,sp\n.set _R1,2\n.endif\n.ifc "
        "%1,gp\n.set _R1,3\n.endif\n.ifc %1,tp\n.set _R1,4\n.endif\n.ifc %1,t0\n.set "
        "_R1,5\n.endif\n.ifc %1,t1\n.set _R1,6\n.endif\n.ifc %1,t2\n.set _R1,7\n.endif\n.ifc "
        "%1,s0\n.set _R1,8\n.endif\n.ifc %1,s1\n.set _R1,9\n.endif\n.ifc %1,a0\n.set "
        "_R1,10\n.endif\n.ifc %1,a1\n.set _R1,11\n.endif\n.ifc %1,a2\n.set "
        "_R1,12\n.endif\n.ifc %1,a3\n.set _R1,13\n.endif\n.ifc %1,a4\n.set "
        "_R1,14\n.endif\n.ifc %1,a5\n.set _R1,15\n.endif\n.ifc %1,a6\n.set "
        "_R1,16\n.endif\n.ifc %1,a7\n.set _R1,17\n.endif\n.ifc %1,s2\n.set "
        "_R1,18\n.endif\n.ifc %1,s3\n.set _R1,19\n.endif\n.ifc %1,s4\n.set "
        "_R1,20\n.endif\n.ifc %1,s5\n.set _R1,21\n.endif\n.ifc %1,s6\n.set "
        "_R1,22\n.endif\n.ifc %1,s7\n.set _R1,23\n.endif\n.ifc %1,s8\n.set "
        "_R1,24\n.endif\n.ifc %1,s9\n.set _R1,25\n.endif\n.ifc %1,s10\n.set "
        "_R1,26\n.endif\n.ifc %1,s11\n.set _R1,27\n.endif\n.ifc %1,t3\n.set "
        "_R1,28\n.endif\n.ifc %1,t4\n.set _R1,29\n.endif\n.ifc %1,t5\n.set "
        "_R1,30\n.endif\n.ifc %1,t6\n.set _R1,31\n.endif\n"
        ".align 2\n"
        ".insn 4, %2 | (_R0 << 7) | (_R1 << 15)\n"
        ".set _R0,z\n.set _R1,z\n" ::"r"(rd),
        "r"(rs1), "i"(encoding));
}
_inst_builder_t __custom_32b_instruction_sb_get(long &rd, long rs1) {
    asm volatile(
        "\n"
        ".ifc %0,zero\n.set _R0,0\n.endif\n.ifc %0,ra\n.set _R0,1\n.endif\n.ifc %0,sp\n.set "
        "_R0,2\n.endif\n.ifc %0,gp\n.set _R0,3\n.endif\n.ifc %0,tp\n.set _R0,4\n.endif\n.ifc "
        "%0,t0\n.set _R0,5\n.endif\n.ifc %0,t1\n.set _R0,6\n.endif\n.ifc %0,t2\n.set "
        "_R0,7\n.endif\n.ifc %0,s0\n.set _R0,8\n.endif\n.ifc %0,s1\n.set _R0,9\n.endif\n.ifc "
        "%0,a0\n.set _R0,10\n.endif\n.ifc %0,a1\n.set _R0,11\n.endif\n.ifc %0,a2\n.set "
        "_R0,12\n.endif\n.ifc %0,a3\n.set _R0,13\n.endif\n.ifc %0,a4\n.set "
        "_R0,14\n.endif\n.ifc %0,a5\n.set _R0,15\n.endif\n.ifc %0,a6\n.set "
        "_R0,16\n.endif\n.ifc %0,a7\n.set _R0,17\n.endif\n.ifc %0,s2\n.set "
        "_R0,18\n.endif\n.ifc %0,s3\n.set _R0,19\n.endif\n.ifc %0,s4\n.set "
        "_R0,20\n.endif\n.ifc %0,s5\n.set _R0,21\n.endif\n.ifc %0,s6\n.set "
        "_R0,22\n.endif\n.ifc %0,s7\n.set _R0,23\n.endif\n.ifc %0,s8\n.set "
        "_R0,24\n.endif\n.ifc %0,s9\n.set _R0,25\n.endif\n.ifc %0,s10\n.set "
        "_R0,26\n.endif\n.ifc %0,s11\n.set _R0,27\n.endif\n.ifc %0,t3\n.set "
        "_R0,28\n.endif\n.ifc %0,t4\n.set _R0,29\n.endif\n.ifc %0,t5\n.set "
        "_R0,30\n.endif\n.ifc %0,t6\n.set _R0,31\n.endif\n.ifc %1,zero\n.set "
        "_R1,0\n.endif\n.ifc %1,ra\n.set _R1,1\n.endif\n.ifc %1,sp\n.set _R1,2\n.endif\n.ifc "
        "%1,gp\n.set _R1,3\n.endif\n.ifc %1,tp\n.set _R1,4\n.endif\n.ifc %1,t0\n.set "
        "_R1,5\n.endif\n.ifc %1,t1\n.set _R1,6\n.endif\n.ifc %1,t2\n.set _R1,7\n.endif\n.ifc "
        "%1,s0\n.set _R1,8\n.endif\n.ifc %1,s1\n.set _R1,9\n.endif\n.ifc %1,a0\n.set "
        "_R1,10\n.endif\n.ifc %1,a1\n.set _R1,11\n.endif\n.ifc %1,a2\n.set "
        "_R1,12\n.endif\n.ifc %1,a3\n.set _R1,13\n.endif\n.ifc %1,a4\n.set "
        "_R1,14\n.endif\n.ifc %1,a5\n.set _R1,15\n.endif\n.ifc %1,a6\n.set "
        "_R1,16\n.endif\n.ifc %1,a7\n.set _R1,17\n.endif\n.ifc %1,s2\n.set "
        "_R1,18\n.endif\n.ifc %1,s3\n.set _R1,19\n.endif\n.ifc %1,s4\n.set "
        "_R1,20\n.endif\n.ifc %1,s5\n.set _R1,21\n.endif\n.ifc %1,s6\n.set "
        "_R1,22\n.endif\n.ifc %1,s7\n.set _R1,23\n.endif\n.ifc %1,s8\n.set "
        "_R1,24\n.endif\n.ifc %1,s9\n.set _R1,25\n.endif\n.ifc %1,s10\n.set "
        "_R1,26\n.endif\n.ifc %1,s11\n.set _R1,27\n.endif\n.ifc %1,t3\n.set "
        "_R1,28\n.endif\n.ifc %1,t4\n.set _R1,29\n.endif\n.ifc %1,t5\n.set "
        "_R1,30\n.endif\n.ifc %1,t6\n.set _R1,31\n.endif\n"
        ".align 2\n"
        ".insn 4, %2 | (_R0 << 7) | (_R1 << 15)\n"
        ".set _R0,z\n.set _R1,z\n"
        : "=r"(rd)
        : "r"(rs1), "i"(encoding));
}
_inst_builder_t __custom_32b_instruction_tcsr_sb_read(long &rd) {
    asm volatile(
        "\n"
        ".ifc %0,zero\n.set _R0,0\n.endif\n.ifc %0,ra\n.set _R0,1\n.endif\n.ifc %0,sp\n.set "
        "_R0,2\n.endif\n.ifc %0,gp\n.set _R0,3\n.endif\n.ifc %0,tp\n.set _R0,4\n.endif\n.ifc "
        "%0,t0\n.set _R0,5\n.endif\n.ifc %0,t1\n.set _R0,6\n.endif\n.ifc %0,t2\n.set "
        "_R0,7\n.endif\n.ifc %0,s0\n.set _R0,8\n.endif\n.ifc %0,s1\n.set _R0,9\n.endif\n.ifc "
        "%0,a0\n.set _R0,10\n.endif\n.ifc %0,a1\n.set _R0,11\n.endif\n.ifc %0,a2\n.set "
        "_R0,12\n.endif\n.ifc %0,a3\n.set _R0,13\n.endif\n.ifc %0,a4\n.set "
        "_R0,14\n.endif\n.ifc %0,a5\n.set _R0,15\n.endif\n.ifc %0,a6\n.set "
        "_R0,16\n.endif\n.ifc %0,a7\n.set _R0,17\n.endif\n.ifc %0,s2\n.set "
        "_R0,18\n.endif\n.ifc %0,s3\n.set _R0,19\n.endif\n.ifc %0,s4\n.set "
        "_R0,20\n.endif\n.ifc %0,s5\n.set _R0,21\n.endif\n.ifc %0,s6\n.set "
        "_R0,22\n.endif\n.ifc %0,s7\n.set _R0,23\n.endif\n.ifc %0,s8\n.set "
        "_R0,24\n.endif\n.ifc %0,s9\n.set _R0,25\n.endif\n.ifc %0,s10\n.set "
        "_R0,26\n.endif\n.ifc %0,s11\n.set _R0,27\n.endif\n.ifc %0,t3\n.set "
        "_R0,28\n.endif\n.ifc %0,t4\n.set _R0,29\n.endif\n.ifc %0,t5\n.set "
        "_R0,30\n.endif\n.ifc %0,t6\n.set _R0,31\n.endif\n"
        ".align 2\n"
        ".insn 4, %1 | (_R0 << 7)\n"
        ".set _R0,z\n"
        : "=r"(rd)
        : "i"(encoding));
}
_inst_builder_t __custom_32b_instruction_tcsr_sb_write(long rd) {
    asm volatile(
        "\n"
        ".ifc %0,zero\n.set _R0,0\n.endif\n.ifc %0,ra\n.set _R0,1\n.endif\n.ifc %0,sp\n.set "
        "_R0,2\n.endif\n.ifc %0,gp\n.set _R0,3\n.endif\n.ifc %0,tp\n.set _R0,4\n.endif\n.ifc "
        "%0,t0\n.set _R0,5\n.endif\n.ifc %0,t1\n.set _R0,6\n.endif\n.ifc %0,t2\n.set "
        "_R0,7\n.endif\n.ifc %0,s0\n.set _R0,8\n.endif\n.ifc %0,s1\n.set _R0,9\n.endif\n.ifc "
        "%0,a0\n.set _R0,10\n.endif\n.ifc %0,a1\n.set _R0,11\n.endif\n.ifc %0,a2\n.set "
        "_R0,12\n.endif\n.ifc %0,a3\n.set _R0,13\n.endif\n.ifc %0,a4\n.set "
        "_R0,14\n.endif\n.ifc %0,a5\n.set _R0,15\n.endif\n.ifc %0,a6\n.set "
        "_R0,16\n.endif\n.ifc %0,a7\n.set _R0,17\n.endif\n.ifc %0,s2\n.set "
        "_R0,18\n.endif\n.ifc %0,s3\n.set _R0,19\n.endif\n.ifc %0,s4\n.set "
        "_R0,20\n.endif\n.ifc %0,s5\n.set _R0,21\n.endif\n.ifc %0,s6\n.set "
        "_R0,22\n.endif\n.ifc %0,s7\n.set _R0,23\n.endif\n.ifc %0,s8\n.set "
        "_R0,24\n.endif\n.ifc %0,s9\n.set _R0,25\n.endif\n.ifc %0,s10\n.set "
        "_R0,26\n.endif\n.ifc %0,s11\n.set _R0,27\n.endif\n.ifc %0,t3\n.set "
        "_R0,28\n.endif\n.ifc %0,t4\n.set _R0,29\n.endif\n.ifc %0,t5\n.set "
        "_R0,30\n.endif\n.ifc %0,t6\n.set _R0,31\n.endif\n"
        ".align 2\n"
        ".insn 4, %1 | (_R0 << 7)\n"
        ".set _R0,z\n" ::"r"(rd),
        "i"(encoding));
}
_inst_builder_t __custom_32b_instruction_vec_ls(long rs2) {
    asm volatile(
        "\n"
        ".ifc %0,zero\n.set _R0,0\n.endif\n.ifc %0,ra\n.set _R0,1\n.endif\n.ifc %0,sp\n.set "
        "_R0,2\n.endif\n.ifc %0,gp\n.set _R0,3\n.endif\n.ifc %0,tp\n.set _R0,4\n.endif\n.ifc "
        "%0,t0\n.set _R0,5\n.endif\n.ifc %0,t1\n.set _R0,6\n.endif\n.ifc %0,t2\n.set "
        "_R0,7\n.endif\n.ifc %0,s0\n.set _R0,8\n.endif\n.ifc %0,s1\n.set _R0,9\n.endif\n.ifc "
        "%0,a0\n.set _R0,10\n.endif\n.ifc %0,a1\n.set _R0,11\n.endif\n.ifc %0,a2\n.set "
        "_R0,12\n.endif\n.ifc %0,a3\n.set _R0,13\n.endif\n.ifc %0,a4\n.set "
        "_R0,14\n.endif\n.ifc %0,a5\n.set _R0,15\n.endif\n.ifc %0,a6\n.set "
        "_R0,16\n.endif\n.ifc %0,a7\n.set _R0,17\n.endif\n.ifc %0,s2\n.set "
        "_R0,18\n.endif\n.ifc %0,s3\n.set _R0,19\n.endif\n.ifc %0,s4\n.set "
        "_R0,20\n.endif\n.ifc %0,s5\n.set _R0,21\n.endif\n.ifc %0,s6\n.set "
        "_R0,22\n.endif\n.ifc %0,s7\n.set _R0,23\n.endif\n.ifc %0,s8\n.set "
        "_R0,24\n.endif\n.ifc %0,s9\n.set _R0,25\n.endif\n.ifc %0,s10\n.set "
        "_R0,26\n.endif\n.ifc %0,s11\n.set _R0,27\n.endif\n.ifc %0,t3\n.set "
        "_R0,28\n.endif\n.ifc %0,t4\n.set _R0,29\n.endif\n.ifc %0,t5\n.set "
        "_R0,30\n.endif\n.ifc %0,t6\n.set _R0,31\n.endif\n"
        ".align 2\n"
        ".insn 4, %1 | (_R0 << 20)\n"
        ".set _R0,z\n" ::"r"(rs2),
        "i"(encoding));
}
_inst_builder_t __custom_32b_instruction_void(void) {
    asm volatile(".align 2\n"
                 ".insn 4, %0\n" ::"i"(encoding));
}
} // namespace BaseNPUInstBuilder

#undef _inst_builder_t

#include "gen_npu_instruction_32.h"
#include "gen_npu_instruction_64.h"
#include "npu_alias.h"

#endif
