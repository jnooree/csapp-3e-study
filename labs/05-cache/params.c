#include "params.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Params params;
const char *progname;

static int parse_str(const char **arg, char argn, const char ***argv,
                     int start) {
  const char *curr = **argv;

  if (curr[start]) {
    *arg = &curr[start];
    return 0;
  }

  *arg = *(++(*argv));
  if (!*arg) {
    printf("%s: option requires an argument -- '%c'\n", progname, argn);
    return kHelp;
  }

  return 0;
}

static int parse_int(int *arg, char argn, const char ***argv, int start) {
  const char *raw;
  int err;

  err = parse_str(&raw, argn, argv, start);
  if (err)
    return err;

  *arg = atoi(raw);
  return errno ? kErr : kOk;
}

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

int parse_args(const char *argv[]) {
  int err = kOk, i, len;
  int index_set = 0, lines_set = 0, offset_set = 0, trace_set = 0;
  const char *arg;
  char c;

  params.verbose = 0;

  for (; (arg = *++argv);)
    if (arg[0] == '-') {
      len = strlen(arg);
      for (i = 1; i < len; ++i) {
        c = arg[i];

        if (c == 'h')
          return 1;

        if (c == 'v') {
          params.verbose = 1;
          continue;
        }

        ++i;
        switch (c) {
        case 's':
          err = parse_int(&params.s, c, &argv, i);
          index_set = !err;
          break;
        case 'E':
          err = parse_int(&params.lines, c, &argv, i);
          lines_set = !err;
          break;
        case 'b':
          err = parse_int(&params.b, c, &argv, i);
          offset_set = !err;
          break;
        case 't':
          err = parse_str(&params.trace, c, &argv, i);
          trace_set = !err;
          break;
        default:
          printf("%s: invalid option -- '%c'\n", progname, c);
          return 1;
        }

        break;
      }

      if (err)
        break;
    }

  err |= !(index_set && lines_set && offset_set && trace_set);
  if (err == kErr)
    printf("%s: Missing required command line argument\n", progname);
  if (err == kOk)
    setup_params();

  return err;
}
