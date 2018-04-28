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
#include "ngtcp2_skl_test.h"

#include <CUnit/CUnit.h>

#include "ngtcp2_skl.h"
#include "ngtcp2_test_helper.h"

typedef struct {
  ngtcp2_skl_node h;
  ngtcp2_skl_node *right[NGTCP2_SKL_MAX_LEVEL + 1];
} skl_node;

void test_ngtcp2_skl_insert(void) {
  ngtcp2_rnd *rnd = static_rnd();
  ngtcp2_skl skl;
  ngtcp2_mem *mem = ngtcp2_mem_default();
  ngtcp2_range keys[] = {{10, 11}, {3, 4},  {8, 9},  {11, 12}, {16, 17},
                         {12, 13}, {1, 2},  {5, 6},  {4, 5},   {0, 1},
                         {13, 14}, {7, 8},  {9, 10}, {2, 3},   {14, 15},
                         {6, 7},   {15, 16}};
  skl_node *nodes[arraylen(keys)];
  size_t i;
  size_t level;
  ngtcp2_skl_node *node;
  ngtcp2_skl_node *pred[NGTCP2_SKL_MAX_LEVEL + 1];
  ngtcp2_range r;

  ngtcp2_skl_init(&skl, NGTCP2_SKL_MAX_LEVEL, rnd, mem);

  for (i = 0; i < arraylen(nodes); ++i) {
    level = ngtcp2_skl_rand_level(&skl);
    nodes[i] = ngtcp2_mem_calloc(mem, 1, sizeof(skl_node));
    ngtcp2_skl_node_init(&nodes[i]->h, &keys[i], level, nodes[i]->right);

    ngtcp2_skl_insert(&skl, &nodes[i]->h, level);
  }

  ngtcp2_range_init(&r, 17, 18);

  CU_ASSERT(NULL == ngtcp2_skl_lower_bound(&skl, pred, &r));

  for (i = 0; i < arraylen(keys); ++i) {
    node = ngtcp2_skl_lower_bound(&skl, pred, &keys[i]);

    CU_ASSERT(NULL != node);

    ngtcp2_skl_remove(&skl, node, pred);
  }

  ngtcp2_skl_free(&skl);

  for (i = 0; i < arraylen(nodes); ++i) {
    ngtcp2_mem_free(mem, nodes[i]);
  }
}
