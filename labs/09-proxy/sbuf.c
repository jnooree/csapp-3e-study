#include "sbuf.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "csapp.h"

struct sbuf {
  char *data;
  size_t n;
  size_t sz;

  size_t front;
  size_t rear;

  pthread_mutex_t mutex;
  sem_t slots;
  sem_t items;
};

sbuf_t sbuf_create(size_t len, size_t size) {
  sbuf_t buf;
  buf = malloc(sizeof(*buf));

  buf->data = calloc(len, size);
  buf->n = len;
  buf->sz = size;

  buf->front = buf->rear = 0;
  pthread_mutex_init(&buf->mutex, NULL);
  sem_init(&buf->slots, 0, len);
  sem_init(&buf->items, 0, 0);

  return buf;
}

void sbuf_destroy(sbuf_t buf) {
  sem_destroy(&buf->items);
  sem_destroy(&buf->slots);
  pthread_mutex_destroy(&buf->mutex);
  free(buf->data);
  free(buf);
}

static void sem_retry_wait(sem_t *sem) {
  int ret;

  while ((ret = sem_wait(sem)) != 0 && errno == EINTR)
    ;

  if (ret != 0)
    unix_error("error waiting semaphore");
}

static void sem_retry_post(sem_t *sem) {
  if (sem_post(sem) != 0)
    unix_error("error posting semaphore");
}

void sbuf_enq(sbuf_t buf, const void *item) {
  sem_retry_wait(&buf->slots);
  pthread_mutex_lock(&buf->mutex);

  memcpy(buf->data + (buf->rear++ % buf->n), item, buf->sz);

  pthread_mutex_unlock(&buf->mutex);
  sem_retry_post(&buf->items);
}

void sbuf_deq(sbuf_t buf, void *item) {
  sem_retry_wait(&buf->items);
  pthread_mutex_lock(&buf->mutex);

  memcpy(item, buf->data + (buf->front++ % buf->n), buf->sz);

  pthread_mutex_unlock(&buf->mutex);
  sem_retry_post(&buf->slots);
}
