#ifndef CSAPP_PROXY_SBUF_H_
#define CSAPP_PROXY_SBUF_H_

#include <stddef.h>

typedef struct sbuf *sbuf_t;

sbuf_t sbuf_create(size_t len, size_t size);
void sbuf_destroy(sbuf_t buf);

void sbuf_enq(sbuf_t buf, const void *item);
void sbuf_deq(sbuf_t buf, void *item);

#endif /* CSAPP_PROXY_SBUF_H_ */
