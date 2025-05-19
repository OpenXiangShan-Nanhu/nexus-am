#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "__gen_simm.h"
#include "ppu.h"

extern const uint16_t MAT_A[M][N];
extern const uint16_t MAT_B[N][M];
extern const uint64_t MAT_REF[M][M];

uint64_t __attribute__((aligned(64))) RES_M[M][M];

void matrix_multiply_uint16_square(
  const uint16_t* restrict A, 
  const uint16_t* restrict B, 
  uint64_t* restrict Z,
  const uint64_t _M, 
  const uint64_t _N,
  const uint64_t _P
);

int check_result(
  const uint64_t* restrict result,
  const uint64_t* restrict ref,
  const int64_t size
);

#define BLOCK_ROWS       (M / NUM_CORES)
#define MAT_A_BLOCK(x)   (&MAT_A[x * BLOCK_ROWS][0])
#define RES_BLOCK(x)     (&RES_M[x * BLOCK_ROWS][0])

int main() {
  uint64_t iam = riscv_mhartid();
  uint8_t err;
  atomic_printf("[INFO]: hart %lu boot\n", iam);

  if(0 == iam) for(int i = 1; i< NUM_CORES; i++) switch_on_core(i);
  err = barrier(NUM_CORES);
  if(err) return err;

  if(0 == iam) atomic_printf("[INFO]: start test!\n");
  matrix_multiply_uint16_square(MAT_A_BLOCK(iam), &MAT_B[0][0], RES_BLOCK(iam), BLOCK_ROWS, N, M);
  riscv_fence();
  while(iam > 0) riscv_wfi();

  if(0 == iam) atomic_printf("[INFO]: start compare!\n");
  err = check_result(&RES_M[0][0], &MAT_REF[0][0], M * M);
  if(err) printf("[ERROR]: result check failed!\n");

  return err;
}