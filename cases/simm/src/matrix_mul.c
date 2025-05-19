#include <stdint.h>
#include <stddef.h>

/**
 * @brief matrix multiply function
 * @param A input matrix  (m x n)
 * @param B output matrix (n x p)
 * @param Z result matrix (m x p)
 * @param m rows number of A
 * @param n cols number of A / rows number of B
 * @param p cols number of B / rows number of Z
*/
void matrix_multiply_uint16_square(
  const uint16_t* restrict A, 
  const uint16_t* restrict B, 
  uint64_t* restrict Z,
  const uint64_t M, 
  const uint64_t N,
  const uint64_t P
) {
  for (size_t m = 0; m < M; m++) {
    for (size_t n = 0; n < N; n++) {
      for (size_t p = 0; p < P; p++) {
        Z[m * P + p] += (uint32_t)A[m * N + n] * (uint32_t)B[n * P + p];
      }
    }
  }
}

int check_result(
  const uint64_t* restrict result,
  const uint64_t* restrict ref,
  const int64_t size
) {
  int64_t ptr = size;
  while(ptr --> 0) {
    if(ref[ptr] != result[ptr]) return -1;
  }
  return 0;
}