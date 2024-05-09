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

#define WSIZE     4
#define DSIZE     8
#define CHUNKSIZE (1 << 12)

#define ALIGNMENT   8
#define SIZE_MASK   ~(ALIGNMENT - 1)
#define ALIGN(size) (((size) + ~SIZE_MASK) & SIZE_MASK)

#define MAX(x, y) ((x) < (y) ? (y) : (x))

#define MIN_ALLOC        ALIGN(sizeof(struct free_list) + DSIZE)
#define ALLOC_SIZE(size) ALIGN(MAX(sizeof(struct free_list), size) + DSIZE)

#define GET(p)    (*(size_t *)(p))
#define PUT(p, v) (*(size_t *)(p) = (v))

#define PACK(size, alloc) ((size) | (alloc))
#define SIZE(mp)          (GET(mp) & SIZE_MASK)
#define ALOC(mp)          (GET(mp) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + SIZE(HDRP(bp)) - DSIZE)
#define META_PUT(bp, size_alloc)                                               \
  do {                                                                         \
    PUT(HDRP(bp), size_alloc);                                                 \
    PUT(FTRP(bp), size_alloc);                                                 \
  } while (0)

#define PREVBLK(bp) ((char *)(bp) - SIZE(HDRP(bp) - WSIZE))
#define NEXTBLK(bp) ((char *)(bp) + SIZE(HDRP(bp)))

struct free_list {
  struct free_list *next;
  struct free_list *prev;
};

struct class_list {
  struct free_list head;
};

static int mm_check(void);

static void zfill_free_node(struct free_list *node) {
#ifdef MM_DEBUG
  size_t bufsz = SIZE(HDRP(node)) - DSIZE - sizeof(*node);

  memset(++node, 0, bufsz);
#endif
  (void)(node);
}

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

static struct class_list *begin_heap = NULL;
static void *end_heap = NULL;

#define class_for(curr, cls)                                                   \
  for ((curr) = begin_heap, (cls) = MIN_ALLOC; (cls) <= CHUNKSIZE;             \
       ++(curr), (cls) <<= 1)

static int isclass(size_t sz, size_t cls) {
  return sz >= cls && (sz < (cls << 1) || cls == CHUNKSIZE);
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
  size_t cls, size;
  struct class_list *clsp;

  size = GET(HDRP(node));

  class_for (clsp, cls)
    if (isclass(size, cls)) {
      free_list_insert(&clsp->head, node);
      zfill_free_node(node);
      return node;
    }

  return NULL;
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

  class_for (clsp, cls) {
    if ((cls << 1) <= size)
      continue;

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
  if (size == 0)
    return NULL;

  size = ALLOC_SIZE(size);
  blk = allocate_block(size);

  if (!mm_check())
    return NULL;

  return blk;
}

void mm_free(void *ptr) {
  if (ptr == NULL)
    return;

  ptr = coalesce_will_free(ptr);
  free_node(ptr);

  mm_check();
}

void *mm_realloc(void *ptr, size_t size) {
  void *oldptr, *newptr;
  size_t copysz;
  int freed = 0;

  if (size == 0) {
    mm_free(ptr);
    return NULL;
  }

  size = ALLOC_SIZE(size);

  if (ptr == NULL) {
    newptr = allocate_block(size);
    if (!mm_check())
      return NULL;
    return newptr;
  }

  oldptr = ptr;
  copysz = SIZE(HDRP(ptr)) - DSIZE;
  ptr = coalesce_will_free(ptr);

  if ((char *)oldptr - (char *)ptr >= copysz) {
    free_node(ptr);
    freed = 1;
  } else if (size <= SIZE(HDRP(ptr))) {
    free_tail_was_resvd(ptr, size);
    if (!mm_check())
      return NULL;
    return ptr;
  }

  newptr = allocate_block(size);
  if (newptr == NULL) {
    if (freed)
      resv_node(ptr);
    uncoalesce_was_resvd(ptr, oldptr, copysz + DSIZE);
    mm_check();
    return NULL;
  }

  memcpy(newptr, oldptr, copysz);
  if (newptr != ptr)
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
  size_t cls;

  class_for (curr, cls) {
    free_for (node, &curr->head) {
      if (ALOC(HDRP(node))) {
        ptr = (char *)node;
        reason = "free node is marked allocated";
        goto fail;
      }

      if (!isclass(SIZE(HDRP(node)), cls)) {
        ptr = (char *)node;
        reason = "free node size is inconsistent with class";
        goto fail;
      }
    }
  }

  for (ptr = (char *)curr + DSIZE; ptr < (char *)end_heap;
       ptr += SIZE(HDRP(ptr))) {
    if (GET(HDRP(ptr)) != GET(FTRP(ptr))) {
      reason = "inconsistent header and footer";
      goto fail;
    }

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
  fprintf(stderr, "error: mm_check: %s (addr: %p, size: %u, allocated: %s)\n",
          reason, (void *)ptr, SIZE(HDRP(ptr)),
          ALOC(HDRP(ptr)) ? "true" : "false");
  return 0;
#endif
}
