#ifndef _NPU_KERNEL_H_
#define _NPU_KERNEL_H_

#include <stdint.h>

#ifdef __cplusplus
#define __device__ extern "C" __attribute__((section(".text.npu_kernel")))
#else
#define __device__ __attribute__((section(".text.npu_kernel")))
#endif

static uint64_t start_cycle;
__attribute__((always_inline)) static inline uint64_t get_mcycle(void) {
    uint64_t cycles;
    asm volatile("csrr %0, mcycle" : "=r"(cycles));
    if(start_cycle == 0) {
        start_cycle = cycles;
    }
    return cycles - start_cycle;
}

__attribute__((always_inline)) static inline uint32_t float_as_u32(float i) {
    union {
        float f;
        uint32_t i;
    } t;
    t.f = i;
    return t.i;
}

__attribute__((always_inline)) static inline float u32_as_float(uint32_t i) {
    union {
        float f;
        uint32_t i;
    } t;
    t.i = i;
    return t.f;
}

#endif
