/*
  Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
  This file is part of Rozo.

  Rozo is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3 of the License,
  or (at your option) any later version.

  Rozo is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see
  <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "log.h"
#include "xmalloc.h"
#include "transform.h"

void transform_forward(const bin_t * support, int rows, int cols, int np,
                       projection_t * projections) {
    int *offsets;
    int i, l, k;
    DEBUG_FUNCTION;

    offsets = xcalloc(np, sizeof (int));
    for (i = 0; i < np; i++) {
        offsets[i] =
            projections[i].angle.p <
            0 ? (rows - 1) * projections[i].angle.p : 0;
        memset(projections[i].bins, 0, projections[i].size * sizeof (bin_t));
    }

    assert(cols % 8 == 0);
    for (i = 0; i < np; i++) {
        projection_t *p = projections + i;
        const pxl_t *ppix = support;
        bin_t *pbin;            // = p->bins - offsets[i];
        for (l = 0; l < rows; l++) {
            pbin = p->bins + l * p->angle.p - offsets[i];
            for (k = cols / 8; k > 0; k--) {
                pbin[0] ^= ppix[0];
                pbin[1] ^= ppix[1];
                pbin[2] ^= ppix[2];
                pbin[3] ^= ppix[3];
                pbin[4] ^= ppix[4];
                pbin[5] ^= ppix[5];
                pbin[6] ^= ppix[6];
                pbin[7] ^= ppix[7];
                pbin += 8;
                ppix += 8;
            }
        }
    }

    if (offsets)
        free(offsets);
}

static int compare_slope(const void *e1, const void *e2) {
    projection_t *p1 = (projection_t *) e1;
    projection_t *p2 = (projection_t *) e2;
    double a = (double) p1->angle.p / (double) p1->angle.q;
    double b = (double) p2->angle.p / (double) p2->angle.q;
    return (a < b) ? 1 : (a > b) ? -1 : 0;
}

static inline int max(int a, int b) {
    return a > b ? a : b;
}

static inline int min(int a, int b) {
    return a < b ? a : b;
}

void transform_inverse(pxl_t * support, int rows, int cols, int np,
                       projection_t * projections) {
    int s_minus, s_plus, s, i, rdv, k, l;
    double tmp;
    int *k_offsets, *offsets;

    k_offsets = xcalloc(np, sizeof (int));
    offsets = xcalloc(np, sizeof (int));

    qsort((void *) projections, np, sizeof (projection_t), compare_slope);
    for (i = 0; i < np; i++) {
        offsets[i] =
            projections[i].angle.p <
            0 ? (rows - 1) * projections[i].angle.p : 0;
    }

    // compute s_minus, s_plus, and finally s
    s_minus = s_plus = s = 0;
    for (i = 1; i < rows - 1; i++) {
        s_minus += max(0, -projections[i].angle.p);
        s_plus += max(0, projections[i].angle.p);
    }
    s = s_minus - s_plus;

    // compute the rendez-vous row rdv
    tmp = pow(((double) projections[0].angle.p - 0.5 * (double) s), 2);
    rdv = 0;
    // XXX
    rdv = rows - 1;
    /*
       for (i = 0; i < rows; i++) {
       double tmpi =
       pow(((double) projections[i].angle.p - 0.5 * (double) s), 2);
       //XXX
       if (tmpi = tmp) {
       tmp = tmpi;
       rdv = i;
       }
       }
     */

    // Determine the initial image column offset for each projection
    k_offsets[rdv] =
        max(max(0, -projections[rdv].angle.p) + s_minus,
            max(0, projections[rdv].angle.p) + s_plus);
    for (i = rdv + 1; i < rows; i++) {
        k_offsets[i] = k_offsets[i - 1] + projections[i - 1].angle.p;
    }
    for (i = rdv - 1; i >= 0; i--) {
        k_offsets[i] = k_offsets[i + 1] + projections[i + 1].angle.p;
    }

    // Reconstruct
    // While all projections aren't needed (avoid if statement in general case)
    for (k = -max(k_offsets[0], k_offsets[rows - 1]); k < 0; k++) {
        for (l = 0; l < rdv; l++) {
            if (k + k_offsets[l] >= 0) {
                projection_t *p = projections + l;
                bin_t bin =
                    projections[l].bins[(k + k_offsets[l]) * p->angle.q +
                                        l * p->angle.p - offsets[l]];
                support[l * cols + k + k_offsets[l]] = bin;
                for (i = 0; i < rows; i++) {
                    projection_t *updated = projections + i;
                    updated->bins[(k + k_offsets[l]) * updated->angle.q +
                                  l * updated->angle.p - offsets[i]] ^= bin;
                }
            }
        }
        for (l = rows - 1; l >= rdv; l--) {
            if (k + k_offsets[l] >= 0) {
                projection_t *p = projections + l;
                bin_t bin =
                    projections[l].bins[(k + k_offsets[l]) * p->angle.q +
                                        l * p->angle.p - offsets[l]];
                support[l * cols + k + k_offsets[l]] = bin;
                for (i = 0; i < rows; i++) {
                    projection_t *updated = projections + i;
                    updated->bins[(k + k_offsets[l]) * updated->angle.q +
                                  l * updated->angle.p - offsets[i]] ^= bin;
                }
            }
        }
    }

    // scan the reconstruction path while every projections are used
    for (k = 0; k < cols - max(k_offsets[0], k_offsets[rows - 1]); k++) {
        for (l = 0; l < rdv; l++) {
            projection_t *p = projections + l;
            bin_t bin =
                projections[l].bins[(k + k_offsets[l]) * p->angle.q +
                                    l * p->angle.p - offsets[l]];
            support[l * cols + k + k_offsets[l]] = bin;
            for (i = 0; i < rows; i++) {
                projection_t *updated = projections + i;
                updated->bins[(k + k_offsets[l]) * updated->angle.q +
                              l * updated->angle.p - offsets[i]] ^= bin;
            }
        }
        for (l = rows - 1; l >= rdv; l--) {
            projection_t *p = projections + l;
            bin_t bin =
                projections[l].bins[(k + k_offsets[l]) * p->angle.q +
                                    l * p->angle.p - offsets[l]];
            support[l * cols + k + k_offsets[l]] = bin;
            for (i = 0; i < rows; i++) {
                projection_t *updated = projections + i;
                updated->bins[(k + k_offsets[l]) * updated->angle.q +
                              l * updated->angle.p - offsets[i]] ^= bin;
            }
        }
    }

    // finished the work
    for (k = cols - max(k_offsets[0], k_offsets[rows - 1]); k < cols; k++) {
        for (l = 0; l < rdv; l++) {
            if (k + k_offsets[l] < cols) {
                projection_t *p = projections + l;
                bin_t bin =
                    projections[l].bins[(k + k_offsets[l]) * p->angle.q +
                                        l * p->angle.p - offsets[l]];
                support[l * cols + k + k_offsets[l]] = bin;
                for (i = 0; i < rows; i++) {
                    projection_t *updated = projections + i;
                    updated->bins[(k + k_offsets[l]) * updated->angle.q +
                                  l * updated->angle.p - offsets[i]] ^= bin;
                }
            }
        }
        for (l = rows - 1; l >= rdv; l--) {
            if (k + k_offsets[l] < cols) {
                projection_t *p = projections + l;
                bin_t bin =
                    projections[l].bins[(k + k_offsets[l]) * p->angle.q +
                                        l * p->angle.p - offsets[l]];
                support[l * cols + k + k_offsets[l]] = bin;
                for (i = 0; i < rows; i++) {
                    projection_t *updated = projections + i;
                    updated->bins[(k + k_offsets[l]) * updated->angle.q +
                                  l * updated->angle.p - offsets[i]] ^= bin;
                }
            }
        }
    }

    if (offsets)
        free(offsets);
    if (k_offsets)
        free(k_offsets);
}
