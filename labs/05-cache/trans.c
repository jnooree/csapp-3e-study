/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */

#include <stddef.h>

#include "cachelab.h"

/* static int is_transpose(int M, int N, int A[N][M], int B[M][N]); */

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - Cache-optimized version
 */

static int get_set_idx(int i, int j, int m, const int *arr) {
  const int *row = &arr[i * m];
  const int *item = row + j;
  size_t addr = (size_t)(item);
  return (addr - (addr >> 10 << 10)) >> 5;
}

static void trans_32(int bsize, int M, int N, int A[N][M], int B[M][N]) {
  int i, j, ii, jj;

  for (jj = 0; jj < M; jj += bsize) {
    for (ii = 0; ii < N; ii += bsize) {
      for (i = ii; i < ii + bsize; i++) {
        for (j = jj; j < jj + bsize; j++)
          if (get_set_idx(i, j, M, *A) != get_set_idx(j, i, N, *B))
            B[j][i] = A[i][j];

        for (j = jj; j < jj + bsize; j++)
          if (get_set_idx(i, j, M, *A) == get_set_idx(j, i, N, *B))
            B[j][i] = A[i][j];
      }
    }
  }
}

static void trans_64(int M, int N, int A[N][M], int B[M][N]) {
  int i, j, k, ii, jj;
  int tmp1, tmp2, tmp1_set;
  const int bsize = 8, bsize_div2 = 4;

  for (jj = 0; jj < M; jj += bsize_div2) {
    for (ii = 0; ii < N; ii += bsize) {
      for (k = 0; k < bsize_div2; k++) {
        tmp1_set = 0;

        for (i = ii + k; i < ii + bsize; i += bsize_div2)
          for (j = jj; j < jj + bsize_div2; j++)
            if (get_set_idx(i, j, M, *A) != get_set_idx(j, i, N, *B)) {
              B[j][i] = A[i][j];
            } else if (tmp1_set) {
              tmp2 = A[i][j];
            } else {
              tmp1 = A[i][j];
              tmp1_set = 1;
            }

        for (i = ii + k; i < ii + bsize; i += bsize_div2) {
          for (j = jj; j < jj + bsize_div2; j++)
            if (get_set_idx(i, j, M, *A) == get_set_idx(j, i, N, *B)) {
              if (tmp1_set) {
                B[j][i] = tmp1;
                tmp1_set = 0;
              } else {
                B[j][i] = tmp2;
              }
            }
        }
      }
    }
  }
}

static void trans_odds(int bsize, int M, int N, int A[N][M], int B[M][N]) {
  int i, j, ii, jj;
  const int en = bsize * (N / bsize + 1), em = bsize * (M / bsize + 1);

  for (jj = 0; jj < em; jj += bsize) {
    for (ii = 0; ii < en; ii += bsize) {
      for (i = ii; i < ii + bsize && i < N; i++)
        for (j = jj; j < jj + bsize && j < M; j++)
          B[j][i] = A[i][j];
    }
  }
}

static void trans_opt(int M, int N, int A[N][M], int B[M][N]) {
  const int bsize = 32 / sizeof(int);

  if (N % bsize)
    trans_odds(bsize, M, N, A, B);
  else if (N == 32)
    trans_32(bsize, M, N, A, B);
  else
    trans_64(M, N, A, B);
}

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
static char *transpose_submit_desc = "Transpose submission";

static void (*transpose_submit)(int M, int N, int A[N][M],
                                int B[M][N]) = trans_opt;

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions(void) {
  /* Register your solution function */
  registerTransFunction(transpose_submit, transpose_submit_desc);
}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
/*
static int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
  int i, j;

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; ++j) {
      if (A[i][j] != B[j][i]) {
        printf("A:\n");
        for (i = 0; i < N; ++i) {
          for (j = 0; j < M; ++j)
            printf("%8x ", A[i][j]);
          printf("\n");
        }
        printf("B^T:\n");
        for (i = 0; i < N; ++i) {
          for (j = 0; j < M; ++j)
            printf("%8x ", B[j][i]);
          printf("\n");
        }
        return 0;
      }
    }
  }
  return 1;
}
*/
