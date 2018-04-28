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
#ifndef NGTCP2_RND_H
#define NGTCP2_RND_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include <ngtcp2/ngtcp2.h>

struct ngtcp2_rnd;
typedef struct ngtcp2_rnd ngtcp2_rnd;

struct ngtcp2_rnd {
  /* data is state of pseudo random generator */
  struct drand48_data data;
  /* xsubi is lat 48-bit Xi generated. */
  unsigned short xsubi[3];
};

void ngtcp2_rnd_init(ngtcp2_rnd *rnd, unsigned short xsubi[3]);

/*
 * ngtcp2_rnd_double returns a uniformly distributed pseudo random
 * number in [0.0, 1.0).
 */
double ngtcp2_rnd_double(ngtcp2_rnd *rnd);

#endif /* NGTCP2_RND_H */
