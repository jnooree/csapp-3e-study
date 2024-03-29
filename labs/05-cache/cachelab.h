/*
 * cachelab.h - Prototypes for Cache Lab helper functions
 */

#ifndef CACHELAB_TOOLS_H
#define CACHELAB_TOOLS_H

#define MAX_TRANS_FUNCS 100

typedef struct trans_func {
  void (*func_ptr)(int M, int N, int[N][M], int[M][N]);
  char *description;
  char correct;
  unsigned int num_hits;
  unsigned int num_misses;
  unsigned int num_evictions;
} trans_func_t;

/*
 * printSummary - This function provides a standard way for your cache
 * simulator * to display its final hit and miss statistics
 */
extern void printSummary(int hits,       /* number of  hits */
                         int misses,     /* number of misses */
                         int evictions); /* number of evictions */

/* Fill the matrix with data */
extern void initMatrix(int M, int N, int A[N][M], int B[M][N]);

/* The baseline trans function that produces correct results. */
extern void correctTrans(int M, int N, int A[N][M], int B[M][N]);

/* Add the given function to the function list */
extern void registerTransFunction(void (*trans)(int M, int N, int[N][M],
                                                int[M][N]),
                                  char *desc);

/* External function defined in trans.c */
extern void registerFunctions(void);

#endif /* CACHELAB_TOOLS_H */
