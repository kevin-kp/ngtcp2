/*
 * ngtcp2
 *
 * Copyright (c) 2018 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "ngtcp2_skl.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "ngtcp2_macro.h"
#include "ngtcp2_mem.h"
#include "ngtcp2_rnd.h"

void ngtcp2_skl_node_init(ngtcp2_skl_node *node, const ngtcp2_range *range,
                          size_t level, ngtcp2_skl_node **right) {
  node->range = *range;
  node->right = right;
  memset(node->right, 0, sizeof(ngtcp2_skl_node *) * (level + 1));
}

void ngtcp2_skl_node_free(ngtcp2_skl_node *node) { (void)node; }

ngtcp2_skl_node *ngtcp2_skl_node_next(ngtcp2_skl_node *node) {
  return node->right[0];
}

void ngtcp2_skl_init(ngtcp2_skl *skl, size_t max_level, ngtcp2_rnd *rnd,
                     ngtcp2_mem *mem) {
  ngtcp2_range r = {0, 0};

  assert(max_level <= NGTCP2_SKL_MAX_LEVEL);

  ngtcp2_skl_node_init(&skl->head, &r, max_level, skl->right);
  skl->rnd = rnd;
  skl->mem = mem;
  skl->max_level = max_level;
  skl->level = 0;
}

void ngtcp2_skl_free(ngtcp2_skl *skl) {
  ngtcp2_skl_node *node, *next;

  for (node = ngtcp2_skl_front(skl); node;) {
    next = ngtcp2_skl_node_next(node);
    ngtcp2_skl_node_free(node);
    ngtcp2_mem_free(skl->mem, node);
    node = next;
  }
}

static int range_intersect(const ngtcp2_range *a, const ngtcp2_range *b) {
  return ngtcp2_max(a->begin, b->begin) < ngtcp2_min(a->end, b->end);
}

void ngtcp2_skl_insert(ngtcp2_skl *skl, ngtcp2_skl_node *node, size_t level) {
  ngtcp2_skl_node *p = &skl->head;
  ssize_t i;
  ngtcp2_skl_node *pred[NGTCP2_SKL_MAX_LEVEL + 1];

  for (i = (ssize_t)skl->level; i >= 0; --i) {
    for (; p->right[i] && p->right[i]->range.begin < node->range.begin &&
           !range_intersect(&p->right[i]->range, &node->range);
         p = p->right[i])
      ;
    pred[i] = p;
  }

  if (level > skl->level) {
    for (i = (ssize_t)skl->level + 1; i <= (ssize_t)level; ++i) {
      pred[i] = &skl->head;
    }

    skl->level = level;
  }

  for (i = 0; i <= (ssize_t)level; ++i) {
    node->right[i] = pred[i]->right[i];
    pred[i]->right[i] = node;
  }
}

void ngtcp2_skl_remove(ngtcp2_skl *skl, ngtcp2_skl_node *node,
                       ngtcp2_skl_node *const *pred) {
  ssize_t i;

  for (i = 0; i <= (ssize_t)skl->level; ++i) {
    if (pred[i]->right[i] != node) {
      break;
    }
    pred[i]->right[i] = node->right[i];
  }

  for (; skl->level > 0 && !skl->head.right[skl->level]; --skl->level)
    ;
}

ngtcp2_skl_node *ngtcp2_skl_lower_bound(ngtcp2_skl *skl, ngtcp2_skl_node **pred,
                                        const ngtcp2_range *range) {
  ngtcp2_skl_node *p = &skl->head;
  ssize_t i;

  for (i = (ssize_t)skl->level; i >= 0; --i) {
    for (; p->right[i] && p->right[i]->range.begin < range->begin &&
           !range_intersect(&p->right[i]->range, range);
         p = p->right[i])
      ;
    pred[i] = p;
  }

  return p->right[0];
}

size_t ngtcp2_skl_rand_level(ngtcp2_skl *skl) {
  size_t level = 0;

  for (;;) {
    if (ngtcp2_rnd_double(skl->rnd) >= 0.5) {
      break;
    }
    if (++level == skl->max_level) {
      break;
    }
  }

  return level;
}

void ngtcp2_skl_print(ngtcp2_skl *skl) {
  size_t i;
  ngtcp2_skl_node *node;

  for (i = 0; i <= skl->level; ++i) {
    node = skl->head.right[i];
    fprintf(stderr, "LV %zu:", i);
    for (; node; node = node->right[i]) {
      fprintf(stderr, " [%lu, %lu)", node->range.begin, node->range.end);
    }
    fprintf(stderr, "\n");
  }
}

ngtcp2_skl_node *ngtcp2_skl_front(ngtcp2_skl *skl) {
  return skl->head.right[0];
}

void ngtcp2_skl_pop_front(ngtcp2_skl *skl) {
  ngtcp2_skl_node *pred[NGTCP2_SKL_MAX_LEVEL + 1];
  size_t i;

  for (i = 0; i <= skl->level; ++i) {
    pred[i] = &skl->head;
  }

  ngtcp2_skl_remove(skl, skl->head.right[0], pred);
}
