#ifndef CSAPP_CACHE_CACHE_H_
#define CSAPP_CACHE_CACHE_H_

#include <stdint.h>

typedef enum {
  kHit = 0x1,
  kMiss = 0x2,
  kEvict = 0x4,
} CacheOpResult;

extern void init_cache(void);
extern void fini_cache(void);

extern CacheOpResult access_cache(uint64_t addr);

#endif /* CSAPP_CACHE_CACHE_H_ */
