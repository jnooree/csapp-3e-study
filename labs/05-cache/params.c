#include "params.h"

#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct Params params;
const char *progname;

static void setup_params(void) {
  uint64_t rev_blkmsk = ~0UL << params.b;

  params.setsz = 1 << params.s;

  params.setmsk = (params.setsz - 1) << params.b;
  params.tagmsk = ~0UL & (~params.setmsk) & rev_blkmsk;
}

void print_help(void) {
  printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n"
         "Options:\n"
         "  -h         Print this help message.\n"
         "  -v         Optional verbose flag.\n"
         "  -s <num>   Number of set index bits.\n"
         "  -E <num>   Number of lines per set.\n"
         "  -b <num>   Number of block offset bits.\n"
         "  -t <file>  Trace file.\n\n"
         "Examples:\n"
         "  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n"
         "  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",
         progname, progname, progname);
}

int parse_args(int argc, char *const argv[]) {
  int err = kOk;
  int index_set = 0, lines_set = 0, offset_set = 0, trace_set = 0;
  char c;

  progname = argv[0];
  params.verbose = 0;

  // NOLINTNEXTLINE(clang-diagnostic-implicit-int-conversion,concurrency-*)
  while ((c = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
    switch (c) {
    case 'h':
      return kHelp;
    case 'v':
      params.verbose = 1;
      break;
    case 's':
      params.s = atoi(optarg);
      index_set = 1;
      break;
    case 'E':
      params.lines = atoi(optarg);
      lines_set = 1;
      break;
    case 'b':
      params.b = atoi(optarg);
      offset_set = 1;
      break;
    case 't':
      params.trace = optarg;
      trace_set = 1;
      break;
    default:
      err = kHelp;
      break;
    }

    if (err)
      break;

    err = errno ? kErr : kOk;
  }

  err |= !(index_set && lines_set && offset_set && trace_set);
  if (err == kErr)
    printf("%s: Missing required command line argument\n", progname);
  else if (err == kOk)
    setup_params();

  return err;
}
