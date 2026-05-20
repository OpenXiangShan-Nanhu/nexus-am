#include <stdint.h>
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "platform.h"

#include "npu_instruction.hpp"

int main() {
    // 仅测试指令发射功能

    // 32 位 TCSR 写指令
    __tx_lsu_tcsr_write<0>(0x12345678);
    __tx_lsu_tcsr_write<1>(0x34567812);
    __tx_lsu_tcsr_write<2>(0x56781234);
    __tx_lsu_tcsr_write<3>(0x78123456);

    // 带依赖的 32 位 TCSR 写指令
    for(int i = 0; i < 1024 ; i ++) {
        __tx_mat_tcsr_write<0>((i * 3) % 129);
        __tx_mat_tcsr_write<1>((i * 5) % 129);
        __tx_mat_tcsr_write<2>((i * 7) % 129);
        __tx_mat_tcsr_write<3>((i * 11) % 129);
    }

    // 32 位 LSU 指令（读 RS1、RD）
    __tx_lsu_dma_u2u<0>(0x0, 0x100);

    // 带依赖的 32 位 LSU 指令
    for(long i = 0; i < 1024 ; i ++) {
        __tx_lsu_dma_u2u<0>((i * 3) % 129, (i * 5) % 129);
        __tx_lsu_dma_u2u<1>((i * 5) % 129, (i * 7) % 129);
        __tx_lsu_dma_u2u<2>((i * 7) % 129, (i * 11) % 129);
        __tx_lsu_dma_u2u<3>((i * 11) % 129, (i * 11) % 129);
    }

    // 32 位 MAT 指令（读 RS1、RS2、RD）

    __tx_mat_mma<0>(0x0, 0x100, 0x200);

    // 带依赖的 32 位 MAT 指令
    for(long i = 0; i < 1024 ; i ++) {
        __tx_mat_mma<0>((i * 3) % 129, (i * 5) % 129, (i * 7) % 129);
        __tx_mat_mma<1>((i * 5) % 129, (i * 7) % 129, (i * 11) % 129);
        __tx_mat_mma<2>((i * 7) % 129, (i * 11) % 129, (i * 13) % 129);
        __tx_mat_mma<3>((i * 11) % 129, (i * 13) % 129, (i * 17) % 129);
    }

    // 64 位 macro 指令（读 RS1、RS2、RD）

    __tx_macro_elemwise_add_fp32<0, 0>((void*)0x0, (void*)0x100, (void*)0x200);

    for(long i = 0; i < 1024 ; i ++) {
        __tx_macro_elemwise_add_fp32<0, 0>((void*)((i * 3) % 129), (void*)((i * 5) % 129), (void*)((i * 7) % 129));
        __tx_macro_elemwise_add_fp32<1, 1>((void*)((i * 5) % 129), (void*)((i * 7) % 129), (void*)((i * 11) % 129));
        __tx_macro_elemwise_add_fp32<2, 2>((void*)((i * 7) % 129), (void*)((i * 11) % 129), (void*)((i * 13) % 129));
        __tx_macro_elemwise_add_fp32<3, 3>((void*)((i * 11) % 129), (void*)((i * 13) % 129), (void*)((i * 17) % 129));
    }

    printf("RVT Instruction Issue Test finished.\n");

    return 0;
}