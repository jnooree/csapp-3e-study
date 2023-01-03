#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cache.h"
#include "cachelab.h"
#include "params.h"

static const char *const kCacheOpName[] = { "hit", "miss", "eviction" };

static void maybe_printf(const char *restrict fmt, ...) {
  va_list args;
  if (!params.verbose)
    return;

  va_start(args, fmt);
  vprintf(fmt, args); // NOLINT(clang-diagnostic-format-nonliteral)
  va_end(args);
}

static void handle_entry(uint64_t addr, int *counts) {
  CacheOpResult result;
  int i, mask;

  result = access_cache(addr);
  for (i = 0, mask = kHit; mask <= kEvict; ++i, mask <<= 1)
    if (mask & result) {
      maybe_printf(" %s", kCacheOpName[i]);
      ++counts[i];
    }
}

static int run(void) {
  FILE *fp;
  char *buf = NULL;
  size_t len = 0;

  char type;
  uint64_t addr, size;
  int counts[3] = { 0, 0, 0 };

  fp = fopen(params.trace, "r");
  if (errno) {
    perror(params.trace);
    return 1;
  }

  while (getline(&buf, &len, fp) >= 0) {
    if (buf[0] != ' ')
      continue;

    sscanf(buf, " %c %lx,%lu", &type, &addr, &size);

    maybe_printf("%c %lx,%lu", type, addr, size);
    handle_entry(addr, counts);
    /* Store after load */
    if (type == 'M')
      handle_entry(addr, counts);
    maybe_printf("\n");
  }

  free(buf);
  fclose(fp);

  printSummary(counts[0], counts[1], counts[2]);
  return 0;
}

int main(int argc, char *const argv[]) {
  int early_stop, ret;

  early_stop = parse_args(argc, argv);
  if (early_stop) {
    print_help();
    return 0;
  }

  init_cache();
  ret = run();
  fini_cache();
  return ret;
}
