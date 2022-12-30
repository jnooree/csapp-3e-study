#include "cache.h"

#include <stdint.h>
#include <stdlib.h>

#include "params.h"

struct CacheLine {
  int64_t tag; /* < 0: invalid, >= 0: valid */
};

struct CacheSet {
  uint64_t *ts;            /* params.lines stamps */
  struct CacheLine *lines; /* params.lines lines */
};

struct Cache {
  struct CacheSet *sets; /* params.setsz sets */
};

struct Cache cache;
static uint64_t tick = 0;

void init_cache(void) {
  uint64_t i, j;

  cache.sets = malloc(sizeof(struct CacheSet) * params.setsz);

  for (i = 0; i < params.setsz; ++i) {
    cache.sets[i].ts = malloc(sizeof(uint64_t) * params.lines);
    cache.sets[i].lines = malloc(sizeof(struct CacheLine) * params.lines);

    for (j = 0; j < params.lines; ++j)
      cache.sets[i].lines[j].tag = -1L;
  }
}

CacheOpResult access_cache(uint64_t addr) {
  int64_t tag = (params.tagmsk & addr) >> 1,
          setid = (params.setmsk & addr) >> params.b;
  int64_t curr_tag;
  struct CacheSet *set = &cache.sets[setid];

  CacheOpResult ret = kMiss;
  uint64_t min_ts = ~0UL;
  int argmin = 0;

  int i;

  for (i = 0; i < params.lines; ++i) {
    curr_tag = set->lines[i].tag;
    if (curr_tag == tag) {
      ret = kHit;
      goto epilog;
    }

    if (curr_tag < 0) {
      min_ts = 0UL;
      argmin = i;
    } else if (min_ts > set->ts[i]) {
      min_ts = set->ts[i];
      argmin = i;
    }
  }

  i = argmin;
  set->lines[i].tag = tag;
  if (min_ts)
    ret |= kEvict;

epilog:
  set->ts[i] = ++tick;
  return ret;
}

void fini_cache(void) {
  uint64_t i;

  for (i = 0; i < params.setsz; ++i) {
    free(cache.sets[i].lines);
    free(cache.sets[i].ts);
  }

  free(cache.sets);
}
