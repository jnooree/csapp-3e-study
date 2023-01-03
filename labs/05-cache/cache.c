#include "cache.h"

#include <stdint.h>
#include <stdlib.h>

#include "params.h"

struct CacheLine {
  int64_t tag; /* < 0: invalid, >= 0: valid */
  uint64_t ts; /* Timestamp */
};

struct CacheSet {
  struct CacheLine *lines; /* params.lines lines */
};

struct Cache {
  struct CacheSet *sets; /* params.setsz sets */
};

static struct Cache cache;
static uint64_t tick = 0;

void init_cache(void) {
  uint64_t i, j;

  cache.sets = malloc(sizeof(struct CacheSet) * params.setsz);

  for (i = 0; i < params.setsz; ++i) {
    cache.sets[i].lines = malloc(sizeof(struct CacheLine) * params.lines);

    for (j = 0; j < params.lines; ++j)
      cache.sets[i].lines[j].tag = -1L;
  }
}

CacheOpResult access_cache(uint64_t addr) {
  int64_t tag = (params.tagmsk & addr) >> 1,
          setid = (params.setmsk & addr) >> params.b;
  struct CacheSet *set = &cache.sets[setid];
  struct CacheLine *curr_line;

  CacheOpResult ret = kMiss;
  uint64_t min_ts = ~0UL;
  int argmin = 0;

  int i;

  for (i = 0; i < params.lines; ++i) {
    curr_line = &set->lines[i];

    // Cache hit
    if (curr_line->tag == tag) {
      ret = kHit;
      goto epilog;
    }

    // Invalid cache line
    if (curr_line->tag < 0) {
      min_ts = 0UL;
      argmin = i;
      break;
    }

    // For LRU policy
    if (min_ts > curr_line->ts) {
      min_ts = curr_line->ts;
      argmin = i;
    }
  }

  curr_line = &set->lines[argmin];
  curr_line->tag = tag;
  if (min_ts)
    ret |= kEvict;

epilog:
  curr_line->ts = ++tick;
  return ret;
}

void fini_cache(void) {
  uint64_t i;

  for (i = 0; i < params.setsz; ++i)
    free(cache.sets[i].lines);

  free(cache.sets);
}
