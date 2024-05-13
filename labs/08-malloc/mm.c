/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include "mm.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
  /* Team name */
  "ateam",
  /* First member's full name */
  "Harry Bovik",
  /* First member's email address */
  "bovik@cs.cmu.edu",
  /* Second member's full name (leave blank if none) */
  "",
  /* Second member's email address (leave blank if none) */
  ""
};

#ifndef MM_LOGLVL
#ifdef MM_DEBUG
#define MM_LOGLVL MM_DEBUG
#else
#define MM_LOGLVL 0
#endif
#endif

// NOLINTBEGIN(clang-diagnostic-gnu-zero-variadic-macro-arguments)

#define mm_log_impl(level, fmt, ...)                                           \
  fprintf(stderr, #level ": %s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define mm_dlog(fmt, ...)                                                      \
  if (MM_LOGLVL > 1)                                                           \
  mm_log_impl(debug, fmt, ##__VA_ARGS__)

#define mm_log(fmt, ...)                                                       \
  if (MM_LOGLVL)                                                               \
  mm_log_impl(info, fmt, ##__VA_ARGS__)

#define mm_error(fmt, ...) mm_log_impl(error, fmt, ##__VA_ARGS__)

// NOLINTEND(clang-diagnostic-gnu-zero-variadic-macro-arguments)

#define WSIZE     4
#define DSIZE     8
#define CHUNKSIZE (1 << 12)

#define ALIGNMENT   8
#define SIZE_MASK   ~(ALIGNMENT - 1)
#define ALIGN(size) (((size) + ~SIZE_MASK) & SIZE_MASK)

#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

#define MIN_ALLOC        ALIGN(sizeof(struct free_list) + DSIZE)
#define ALLOC_SIZE(size) ALIGN(MAX(sizeof(struct free_list), size) + DSIZE)

#define GET(p)    (*(size_t *)(p))
#define PUT(p, v) (*(size_t *)(p) = (v))

#define PACK(size, alloc) ((size) | (alloc))
#define SIZE(mp)          (GET(mp) & SIZE_MASK)
#define ALOC(mp)          (GET(mp) & 0x1)

#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define PFTRP(bp) ((char *)(bp) - DSIZE)

#define PREVBLK(bp) ((char *)(bp) - SIZE(PFTRP(bp)))
#define NEXTBLK(bp) ((char *)(bp) + SIZE(HDRP(bp)))

#define FTRP(bp) PFTRP(NEXTBLK(bp))

#define META_PUT(bp, size_alloc)                                               \
  do {                                                                         \
    PUT(HDRP(bp), size_alloc);                                                 \
    PUT(FTRP(bp), size_alloc);                                                 \
  } while (0)

static const char *printbool(int b) {
  return b ? "true" : "false";
}

struct free_list {
  struct free_list *next;
  struct free_list *prev;
};

struct class_list {
  struct free_list head;
};

static struct class_list *begin_heap = NULL;
static void *end_heap = NULL;

static int mm_check(void);

#define free_for(list, head)                                                   \
  for ((list) = (head)->next; (list) != (head); (list) = (list)->next)

static struct free_list *free_list_init(void *ptr) {
  struct free_list *fl = ptr;
  fl->next = fl->prev = fl;
  return fl;
}

static struct free_list *free_list_insert(struct free_list *head, void *buf) {
  struct free_list *node = buf;

  node->next = head->next;
  node->prev = head;

  head->next->prev = node;
  head->next = node;

  return node;
}

static void *free_list_remove(struct free_list *node) {
  node->next->prev = node->prev;
  node->prev->next = node->next;
  return node;
}

#define class_for_from(curr, curr_init, cls)                                   \
  for ((curr) = (curr_init), (cls) = MIN_ALLOC << ((curr) - begin_heap);       \
       (cls) <= CHUNKSIZE; ++(curr), (cls) <<= 1)

#define class_for(curr, cls) class_for_from (curr, begin_heap, cls)

static int last_set_bit(unsigned int arg) {
  return CHAR_BIT * (int)sizeof(arg) - __builtin_clz(arg) - 1;
}

static struct class_list *classof(size_t sz) {
  int lastbit, minbit, maxbit, clsidx;

  lastbit = last_set_bit(sz);

  minbit = last_set_bit(MIN_ALLOC);
  maxbit = last_set_bit(CHUNKSIZE);

  clsidx = MIN(lastbit - minbit, maxbit - minbit);
  if (clsidx < 0)
    mm_log("block size underflow");

  return begin_heap + clsidx;
}

static void class_list_init(void) {
  size_t cls;
  struct class_list *clsp;

  class_for (clsp, cls)
    free_list_init(&clsp->head);
}

static void *resv_node(struct free_list *node) {
  return free_list_remove(node);
}

static struct free_list *free_node(void *node) {
  size_t size;
  struct class_list *clsp;

  size = GET(HDRP(node));
  clsp = classof(size);
  free_list_insert(&clsp->head, node);
  return node;
}

static void *coalesce_will_free(void *ptr) {
  void *prev, *next;
  size_t blksz;

  blksz = SIZE(HDRP(ptr));

  prev = PREVBLK(ptr);
  next = NEXTBLK(ptr);

  if (!ALOC(HDRP(prev))) {
    resv_node(prev);
    ptr = prev;
    blksz += GET(HDRP(prev));
  }

  if (!ALOC(HDRP(next))) {
    resv_node(next);
    blksz += GET(HDRP(next));
  }

  META_PUT(ptr, blksz);
  return ptr;
}

static void *uncoalesce_was_resvd(void *newblk, void *oldblk, size_t oldsz) {
  void *prev, *next;
  size_t newsz, prevsz, nextsz;

  newsz = SIZE(HDRP(newblk));

  META_PUT(oldblk, PACK(oldsz, 1));
  prev = PREVBLK(oldblk);
  next = NEXTBLK(oldblk);

  prevsz = (char *)oldblk - (char *)prev;
  nextsz = newsz - oldsz - prevsz;

  if (prevsz > 0) {
    META_PUT(prev, prevsz);
    free_node(prev);
  }

  if (nextsz > 0) {
    META_PUT(next, nextsz);
    free_node(next);
  }

  return oldblk;
}

static struct free_list *extend_heap(size_t minsize, void *oldend) {
  void *beginp;
  size_t reqsz;

  if (oldend != NULL && !ALOC(PFTRP(oldend)))
    minsize -= GET(PFTRP(oldend));

  reqsz = (MAX(CHUNKSIZE, minsize) + (CHUNKSIZE - 1)) & ~(CHUNKSIZE - 1);

  beginp = mem_sbrk(reqsz);
  if (beginp == (void *)-1)
    return NULL;

  end_heap = (char *)beginp + reqsz;
  PUT(HDRP(end_heap), PACK(WSIZE, 1));

  if (oldend != NULL) {
    beginp = oldend;
    META_PUT(beginp, reqsz);
    beginp = coalesce_will_free(beginp);
  }

  return beginp;
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  void *beginp;
  char *blockp;
  size_t cls, prosz, blksz;

  mm_log("init\n");

  beginp = extend_heap(CHUNKSIZE, NULL);
  if (beginp == NULL)
    return -1;

  prosz = DSIZE;
  for (cls = MIN_ALLOC; cls <= CHUNKSIZE; cls <<= 1)
    prosz += sizeof(struct class_list);

  // reserve for prolog (prosz)
  blockp = (void *)ALIGN((size_t)((char *)beginp + prosz));

  // prolog
  META_PUT(blockp - prosz, PACK(prosz, 1));
  begin_heap = (struct class_list *)(blockp - prosz);
  class_list_init();

  blksz = (char *)end_heap - blockp;
  META_PUT(blockp, blksz);
  free_node(blockp);

  if (!mm_check())
    return -1;

  return 0;
}

static struct free_list *free_tail_was_resvd(void *blk, size_t req) {
  struct free_list *tail;
  size_t blksz, second;

  blksz = SIZE(HDRP(blk));
  second = blksz - req;

  if (second < MIN_ALLOC)
    req = blksz;

  META_PUT(blk, PACK(req, 1));

  if (second < MIN_ALLOC)
    return NULL;

  tail = (struct free_list *)((char *)blk + req);
  META_PUT(tail, second);
  free_node(tail);
  return tail;
}

static void *allocate_block(size_t size) {
  struct class_list *clsp;
  struct free_list *list, *node;
  size_t cls;

  node = NULL;

  class_for_from (clsp, classof(size), cls) {
    free_for (list, &clsp->head)
      if (GET(HDRP(list)) >= size) {
        node = list;
        break;
      }

    if (node != NULL)
      break;
  }

  if (node != NULL) {
    resv_node(node);
  } else {
    node = extend_heap(size, end_heap);
    if (node == NULL)
      return NULL;
  }

  free_tail_was_resvd(node, size);
  return node;
}

void *mm_malloc(size_t size) {
  void *blk;

  mm_log("size: 0x%x\n", size);

  if (size == 0)
    return NULL;

  size = ALLOC_SIZE(size);
  blk = allocate_block(size);

  if (blk && !mm_check())
    return NULL;

  return blk;
}

__nonnull() static void mm_free_nonnull(void *ptr) {
  mm_log("ptr: %p\n", ptr);

  ptr = coalesce_will_free(ptr);
  free_node(ptr);

  mm_check();
}

void mm_free(void *ptr) {
  if (ptr == NULL)
    return;

  mm_free_nonnull(ptr);
}

void *mm_realloc(void *ptr, size_t size) {
  void *oldptr, *newptr;
  size_t oldsz, copysz;

  mm_log("ptr: %p, size: 0x%x\n", ptr, size);

  if (ptr == NULL)
    return mm_malloc(size);

  if (size == 0) {
    mm_free_nonnull(ptr);
    return NULL;
  }

  size = ALLOC_SIZE(size);

  oldptr = ptr;
  oldsz = SIZE(HDRP(oldptr));
  copysz = MIN(oldsz, size) - DSIZE;

  ptr = coalesce_will_free(oldptr);

  if (size <= GET(HDRP(ptr))) {
    memmove(ptr, oldptr, copysz);
    free_tail_was_resvd(ptr, size);

    if (!mm_check())
      return NULL;

    return ptr;
  }

  // prevent coalescing
  META_PUT(ptr, PACK(GET(HDRP(ptr)), 1));

  newptr = allocate_block(size);
  if (newptr == NULL) {
    uncoalesce_was_resvd(ptr, oldptr, oldsz);
    return NULL;
  }

  memcpy(newptr, oldptr, copysz);

  META_PUT(ptr, SIZE(HDRP(ptr)));
  free_node(ptr);

  if (!mm_check())
    return NULL;

  return newptr;
}

static int mm_check(void) {
#ifdef MM_DEBUG
  struct class_list *curr;
  struct free_list *node;
  const char *reason;
  char *ptr;
  size_t cls, size;

  class_for (curr, cls) {
    mm_dlog("cls: %x\n", cls);

    free_for (node, &curr->head) {
      if (ALOC(HDRP(node))) {
        ptr = (char *)node;
        reason = "free node is marked allocated";
        goto fail;
      }

      size = SIZE(HDRP(node));
      if (size < cls || (size >= (cls << 1) && cls != CHUNKSIZE)) {
        ptr = (char *)node;
        reason = "free node size is inconsistent with class";
        goto fail;
      }

      mm_dlog("\tnode: %p: size: 0x%x\n", (void *)node, SIZE(HDRP(node)));
    }
  }

  for (ptr = (char *)curr + DSIZE; ptr < (char *)end_heap;
       ptr += SIZE(HDRP(ptr))) {
    if (GET(HDRP(ptr)) != GET(FTRP(ptr))) {
      reason = "inconsistent header and footer";
      goto fail;
    }

    mm_log("node: %p: size: 0x%x, allocated: %s\n", (void *)ptr,
           SIZE(HDRP(ptr)), printbool(ALOC(HDRP(ptr))));

    if (ALOC(HDRP(ptr)))
      continue;

    node = (struct free_list *)ptr;
    if (node->next->prev != node || node->prev->next != node) {
      reason = "malformed free list";
      goto fail;
    }
  }
#endif

  return 1;

#ifdef MM_DEBUG
fail:
  mm_error("%s (addr: %p, size: %u, allocated: %s)\n", reason, (void *)ptr,
           SIZE(HDRP(ptr)), printbool(ALOC(HDRP(ptr))));
  return 0;
#endif
}
