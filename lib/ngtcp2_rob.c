/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
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
#include "ngtcp2_rob.h"

#include <string.h>
#include <assert.h>

#include "ngtcp2_macro.h"

int ngtcp2_rob_gap_new(ngtcp2_rob_gap **pg, uint64_t begin, uint64_t end,
                       size_t level, ngtcp2_mem *mem) {
  ngtcp2_range range = {begin, end};

  *pg = ngtcp2_mem_malloc(mem, sizeof(ngtcp2_rob_gap) +
                                   sizeof(ngtcp2_skl_node *) * (level + 1));
  if (*pg == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  ngtcp2_skl_node_init(
      &(*pg)->node, &range, level,
      (ngtcp2_skl_node **)(void *)((uint8_t *)(*pg) + sizeof(ngtcp2_rob_gap)));
  (*pg)->level = level;

  return 0;
}

void ngtcp2_rob_gap_del(ngtcp2_rob_gap *g, ngtcp2_mem *mem) {
  ngtcp2_mem_free(mem, g);
}

int ngtcp2_rob_data_new(ngtcp2_rob_data **pd, uint64_t offset, size_t chunk,
                        size_t level, ngtcp2_mem *mem) {
  ngtcp2_range range = {offset, offset + chunk};

  *pd = ngtcp2_mem_malloc(mem, sizeof(ngtcp2_rob_data) +
                                   sizeof(ngtcp2_skl_node *) * (level + 1) +
                                   chunk);
  if (*pd == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  ngtcp2_skl_node_init(
      &(*pd)->node, &range, level,
      (ngtcp2_skl_node **)(void *)((uint8_t *)(*pd) + sizeof(ngtcp2_rob_data)));
  (*pd)->begin = (uint8_t *)(*pd) + sizeof(ngtcp2_rob_data) +
                 sizeof(ngtcp2_skl_node *) * (level + 1);
  (*pd)->end = (*pd)->begin + chunk;
  (*pd)->level = level;

  return 0;
}

void ngtcp2_rob_data_del(ngtcp2_rob_data *d, ngtcp2_mem *mem) {
  ngtcp2_mem_free(mem, d);
}

int ngtcp2_rob_init(ngtcp2_rob *rob, size_t chunk, ngtcp2_rnd *rnd,
                    ngtcp2_mem *mem) {
  int rv;
  ngtcp2_rob_gap *g;

  ngtcp2_skl_init(&rob->gapskl, 32, rnd, mem);

  rv = ngtcp2_rob_gap_new(&g, 0, UINT64_MAX,
                          ngtcp2_skl_rand_level(&rob->gapskl), mem);
  if (rv != 0) {
    return rv;
  }

  ngtcp2_skl_insert(&rob->gapskl, &g->node, g->level);

  ngtcp2_skl_init(&rob->dataskl, 19, rnd, mem);

  rob->chunk = chunk;
  rob->mem = mem;

  return 0;
}

void ngtcp2_rob_free(ngtcp2_rob *rob) {
  if (rob == NULL) {
    return;
  }

  ngtcp2_skl_free(&rob->dataskl);
  ngtcp2_skl_free(&rob->gapskl);
}

static int rob_write_data(ngtcp2_rob *rob, ngtcp2_rob_data **pd,
                          uint64_t offset, const uint8_t *data, size_t len) {
  size_t n;
  int rv;
  ngtcp2_rob_data *nd;
  ngtcp2_skl_node *dn;
  ngtcp2_range range = {offset, offset + len};
  ngtcp2_skl_node *dpred[NGTCP2_SKL_MAX_LEVEL + 1];

  if (*pd == NULL) {
    dn = ngtcp2_skl_lower_bound(&rob->dataskl, dpred, &range);
    if (dn) {
      *pd = ngtcp2_struct_of(dn, ngtcp2_rob_data, node);
    }
  }

  for (;;) {
    if (*pd == NULL || offset < (*pd)->node.range.begin) {
      rv = ngtcp2_rob_data_new(&nd, (offset / rob->chunk) * rob->chunk,
                               rob->chunk, ngtcp2_skl_rand_level(&rob->dataskl),
                               rob->mem);
      if (rv != 0) {
        return rv;
      }
      ngtcp2_skl_insert(&rob->dataskl, &nd->node, nd->level);
      *pd = nd;
    } else if ((*pd)->node.range.begin + rob->chunk < offset) {
      assert(0);
    }
    n = ngtcp2_min(len, (*pd)->node.range.begin + rob->chunk - offset);
    memcpy((*pd)->begin + (offset - (*pd)->node.range.begin), data, n);
    offset += n;
    data += n;
    len -= n;
    if (len == 0) {
      return 0;
    }
    dn = ngtcp2_skl_node_next(&(*pd)->node);
    if (dn) {
      *pd = ngtcp2_struct_of(dn, ngtcp2_rob_data, node);
    } else {
      *pd = NULL;
    }
  }

  return 0;
}

int ngtcp2_rob_push(ngtcp2_rob *rob, uint64_t offset, const uint8_t *data,
                    size_t datalen) {
  int rv;
  ngtcp2_rob_gap *g;
  ngtcp2_range m, l, r, q = {offset, offset + datalen};
  ngtcp2_rob_data *d = NULL;
  ngtcp2_skl_node *gn, *ngn;
  ngtcp2_skl_node *gpred[NGTCP2_SKL_MAX_LEVEL + 1];
  int stale = 0;

  gn = ngtcp2_skl_lower_bound(&rob->gapskl, gpred, &q);
  if (!gn) {
    return 0;
  }
  for (;;) {
    g = ngtcp2_struct_of(gn, ngtcp2_rob_gap, node);

    m = ngtcp2_range_intersect(&q, &gn->range);
    if (!ngtcp2_range_len(&m)) {
      break;
    }
    if (ngtcp2_range_equal(&gn->range, &m)) {
      ngn = ngtcp2_skl_node_next(gn);
      if (stale) {
        ngtcp2_skl_lower_bound(&rob->gapskl, gpred, &q);
      }
      ngtcp2_skl_remove(&rob->gapskl, gn, gpred);
      ngtcp2_rob_gap_del(g, rob->mem);
      rv = rob_write_data(rob, &d, m.begin, data + (m.begin - offset),
                          ngtcp2_range_len(&m));
      if (rv != 0) {
        return rv;
      }
      gn = ngn;
      stale = 1;
      continue;
    }
    ngtcp2_range_cut(&l, &r, &gn->range, &m);
    if (ngtcp2_range_len(&l)) {
      gn->range = l;

      if (ngtcp2_range_len(&r)) {
        ngtcp2_rob_gap *ng;
        rv = ngtcp2_rob_gap_new(&ng, r.begin, r.end,
                                ngtcp2_skl_rand_level(&rob->gapskl), rob->mem);
        if (rv != 0) {
          return rv;
        }
        ngtcp2_skl_insert(&rob->gapskl, &ng->node, ng->level);
      }
    } else if (ngtcp2_range_len(&r)) {
      gn->range = r;
    }
    rv = rob_write_data(rob, &d, m.begin, data + (m.begin - offset),
                        ngtcp2_range_len(&m));
    if (rv != 0) {
      return rv;
    }
    gn = ngtcp2_skl_node_next(gn);
    if (!gn) {
      break;
    }
    stale = 1;
  }
  return 0;
}

void ngtcp2_rob_remove_prefix(ngtcp2_rob *rob, uint64_t offset) {
  ngtcp2_skl_node *gn, *dn;

  for (;;) {
    gn = ngtcp2_skl_front(&rob->gapskl);
    if (!gn) {
      break;
    }
    if (offset <= gn->range.begin) {
      break;
    }
    if (offset < gn->range.end) {
      gn->range.begin = offset;
      break;
    }
    ngtcp2_skl_pop_front(&rob->gapskl);
    ngtcp2_rob_gap_del(ngtcp2_struct_of(gn, ngtcp2_rob_gap, node), rob->mem);
  }

  for (;;) {
    dn = ngtcp2_skl_front(&rob->dataskl);
    if (!dn) {
      return;
    }
    if (offset <= dn->range.begin) {
      return;
    }
    if (offset < dn->range.begin + rob->chunk) {
      return;
    }
    ngtcp2_skl_pop_front(&rob->dataskl);
    ngtcp2_rob_data_del(ngtcp2_struct_of(dn, ngtcp2_rob_data, node), rob->mem);
  }
}

size_t ngtcp2_rob_data_at(ngtcp2_rob *rob, const uint8_t **pdest,
                          uint64_t offset) {
  ngtcp2_rob_data *d;
  ngtcp2_skl_node *gn, *dn;

  gn = ngtcp2_skl_front(&rob->gapskl);
  if (!gn) {
    return 0;
  }

  if (gn->range.begin <= offset) {
    return 0;
  }

  dn = ngtcp2_skl_front(&rob->dataskl);

  assert(dn);

  assert(dn->range.begin <= offset);
  assert(offset < dn->range.begin + rob->chunk);

  d = ngtcp2_struct_of(dn, ngtcp2_rob_data, node);

  *pdest = d->begin + (offset - dn->range.begin);

  return ngtcp2_min(gn->range.begin, dn->range.begin + rob->chunk) - offset;
}

void ngtcp2_rob_pop(ngtcp2_rob *rob, uint64_t offset, size_t len) {
  ngtcp2_skl_node *dn = ngtcp2_skl_front(&rob->dataskl);

  assert(dn);

  if (offset + len < dn->range.begin + rob->chunk) {
    return;
  }

  ngtcp2_skl_pop_front(&rob->dataskl);
  ngtcp2_rob_data_del(ngtcp2_struct_of(dn, ngtcp2_rob_data, node), rob->mem);
}

uint64_t ngtcp2_rob_first_gap_offset(ngtcp2_rob *rob) {
  ngtcp2_skl_node *gn = ngtcp2_skl_front(&rob->gapskl);

  if (!gn) {
    return UINT64_MAX;
  }
  return gn->range.begin;
}
