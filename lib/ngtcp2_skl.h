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
#ifndef NGTCP2_SKL_H
#define NGTCP2_SKL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include <ngtcp2/ngtcp2.h>

#include "ngtcp2_range.h"

/*
 * Skip List implementation inspired by
 * https://www.geeksforgeeks.org/skip-list-set-2-insertion/
 */

/* NGTCP2_SKL_MAX_LEVEL is the maximum */
#define NGTCP2_SKL_MAX_LEVEL 32

struct ngtcp2_rnd;
typedef struct ngtcp2_rnd ngtcp2_rnd;

struct ngtcp2_skl_node;
typedef struct ngtcp2_skl_node ngtcp2_skl_node;

struct ngtcp2_skl_node {
  ngtcp2_range range;
  /* right[i] points to the right (next) node at level i. */
  ngtcp2_skl_node **right;
};

struct ngtcp2_skl;
typedef struct ngtcp2_skl ngtcp2_skl;

struct ngtcp2_skl {
  ngtcp2_skl_node head;
  ngtcp2_rnd *rnd;
  ngtcp2_mem *mem;
  /* max_level is the maximum level of skip list.  It must satisfy
     max_level <= NGTCP2_SKL_MAX_LEVEL. */
  size_t max_level;
  /* level is the highest level at the moment.  level starts from
     0. */
  size_t level;
  /* right is a buffer for head.right. */
  ngtcp2_skl_node *right[NGTCP2_SKL_MAX_LEVEL + 1];
};

/*
 * ngtcp2_skl_node_init initializes |node|.  |level| is a level of
 * this node.  |right| must point to buffer which can contain at least
 * |level| + 1 pointers to ngtcp2_skl_node.
 */
void ngtcp2_skl_node_init(ngtcp2_skl_node *node, const ngtcp2_range *range,
                          size_t level, ngtcp2_skl_node **right);

/*
 * ngtcp2_skl_node_free frees resource allocated for |node|.  It does
 * not free the memory region pointed by |node| itself.
 */
void ngtcp2_skl_node_free(ngtcp2_skl_node *node);

/*
 * ngtcp2_skl_node_next returns the next node of |node| in level 0.
 * If there is no such node, it returns NULL.
 */
ngtcp2_skl_node *ngtcp2_skl_node_next(ngtcp2_skl_node *node);

/*
 * ngtcp2_skl_init initializes |skl|.  |max_level| is the maximum
 * level of skip list.  |rnd| is an initialized pseudo random
 * generator.
 */
void ngtcp2_skl_init(ngtcp2_skl *skl, size_t max_level, ngtcp2_rnd *rnd,
                     ngtcp2_mem *mem);

/*
 * ngtcp2_skl_free frees resources allocated for |skl|.  It does not
 * free the memory region pointed by |skl| itself.
 */
void ngtcp2_skl_free(ngtcp2_skl *skl);

/*
 * ngtcp2_skl_insert inserts |node| into |skl|.  |level| is a level of
 * |node|.  On successful insertion, |skl| owns |node|, so caller must
 * not free |node| unless it is removed by ngtcp2_skl_remove.
 */
void ngtcp2_skl_insert(ngtcp2_skl *skl, ngtcp2_skl_node *node, size_t level);

/*
 * ngtcp2_skl_remove removes |node| from |skl|.  On successful
 * removal, |skl| releases ownership of |node|, so caller must free
 * |node| if it is allocated dynamically.  This function assumes that
 * |skl| contains |node|.  |pred| must be filled with all pointers in
 * every level which point to node.  It can be made by calling
 * ngtcp2_skl_lower_bound.
 */
void ngtcp2_skl_remove(ngtcp2_skl *skl, ngtcp2_skl_node *node,
                       ngtcp2_skl_node *const *pred);

/*
 * ngtcp2_skl_lower_bound returns a node which has the minimum offset
 * that follows or intersects |range|.  |pred| will be filled with all
 * pointers in every level which point to the returned node.  If no
 * such node is found, the function returns NULL.
 */
ngtcp2_skl_node *ngtcp2_skl_lower_bound(ngtcp2_skl *skl, ngtcp2_skl_node **pred,
                                        const ngtcp2_range *range);

/*
 * ngtcp2_skl_rand_level returns a number which is suitable to be used
 * as level.
 */
size_t ngtcp2_skl_rand_level(ngtcp2_skl *skl);

/*
 * ngtcp2_skl_print prints the current state of |skl| to stderr.
 */
void ngtcp2_skl_print(ngtcp2_skl *skl);

/*
 * ngtcp2_skl_front returns the first node in |skl|.  If there is no
 * such node, it returns NULL.
 */
ngtcp2_skl_node *ngtcp2_skl_front(ngtcp2_skl *skl);

/*
 * ngtcp2_skl_pop_front removes the first node from |skl|.  This
 * function assumes the existence of the first node.  This function
 * does not free the removed node.
 */
void ngtcp2_skl_pop_front(ngtcp2_skl *skl);

#endif /* NGTCP2_SKL_H */
