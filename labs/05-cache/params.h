#ifndef CSAPP_CACHE_ARGPARSE_H_
#define CSAPP_CACHE_ARGPARSE_H_

#include <stdint.h>

struct Params {
  int verbose;

  int s;
  int lines;
  int b;
  const char *trace;

  uint64_t setsz;
  uint64_t setmsk;
  uint64_t tagmsk;
};

extern struct Params params;

enum Retvals {
  kOk = 0x0,
  kErr = 0x1,
  kHelp = 0x2,
};

extern const char *progname;

extern void print_help(void);
extern int parse_args(const char *argv[]);

#endif /* CSAPP_CACHE_ARGPARSE_H_ */
