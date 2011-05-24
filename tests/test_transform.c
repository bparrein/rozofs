#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "transform.h"

static pxl_t ref_support[] = { 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3,
    2, 3, 8, 4, 6, 2, 6, 4
};
static bin_t ref_p0_bins[] = { 2, 6, 8, 0, 10, 10, 4, 4, 1, 6 };
static bin_t ref_p1_bins[] = { 4, 1, 9, 13, 10, 12, 13, 1 };
static bin_t ref_p2_bins[] = { 3, 4, 5, 7, 5, 4, 3, 13, 5, 4 };

static pxl_t *support = NULL;
static projection_t *projections;

int test_transform_initialize(void);
int test_transform_release(void);
int test_transform_forward(void);
int test_transform_inverse(void);

int test_transform_initialize(void) {
    int status;

    support = malloc(24 * sizeof (pxl_t));
    if (support == NULL) {
        status = -1;
        goto out;
    }

    projections = malloc(3 * sizeof (projection_t));
    if (projections == NULL) {
        test_transform_release();
        status = -1;
        goto out;
    }
    projections[0].bins = NULL;
    projections[1].bins = NULL;
    projections[2].bins = NULL;

    projections[0].angle.p = -1;
    projections[0].angle.q = 1;
    projections[0].size = 10;
    projections[0].bins = malloc(10 * sizeof (bin_t));
    if (projections[0].bins == NULL) {
        test_transform_release();
        status = -1;
        goto out;
    }

    projections[1].angle.p = 0;
    projections[1].angle.q = 1;
    projections[1].size = 8;
    projections[1].bins = malloc(8 * sizeof (bin_t));
    if (projections[1].bins == NULL) {
        test_transform_release();
        status = -1;
        goto out;
    }

    projections[2].angle.p = 1;
    projections[2].angle.q = 1;
    projections[2].size = 10;
    projections[2].bins = malloc(10 * sizeof (bin_t));
    if (projections[2].bins == NULL) {
        test_transform_release();
        status = -1;
        goto out;
    }

    status = 0;
out:
    return status;
}

int test_transform_release(void) {

    if (support != NULL) {
        free(support);
    }

    if (projections != NULL) {
        if (projections[0].bins != NULL)
            free(projections[0].bins);
        if (projections[1].bins != NULL)
            free(projections[1].bins);
        if (projections[2].bins != NULL)
            free(projections[2].bins);
        free(projections);
    }

    return 0;
}

int test_transform_forward(void) {
    int status;

    memcpy(support, ref_support, 24 * sizeof (pxl_t));
    transform_forward(support, 3, 8, 3, projections);
    status = -!(memcmp(projections[0].bins, ref_p0_bins, 10 * sizeof (bin_t))
                == 0 &&
                memcmp(projections[1].bins, ref_p1_bins, 8 * sizeof (bin_t))
                == 0 &&
                memcmp(projections[2].bins, ref_p2_bins, 10 * sizeof (bin_t))
                == 0);

    return status;
}

int test_transform_inverse(void) {
    int status;

    memcpy(projections[0].bins, ref_p0_bins, 10 * sizeof (bin_t));
    memcpy(projections[1].bins, ref_p1_bins, 8 * sizeof (bin_t));
    memcpy(projections[2].bins, ref_p2_bins, 10 * sizeof (bin_t));

    transform_inverse(support, 3, 8, 3, projections);

    status = -!(memcmp(support, ref_support, 24 * sizeof (bin_t)) == 0);

    return status;
}

int main(int argc, char **argv) {

    if (test_transform_initialize() != 0) {
        perror("Failed to initialize test");
        test_transform_release();
        exit(-1);
    }

    if (test_transform_forward() != 0) {
        perror("Failed to test forward");
        test_transform_release();
        exit(-1);
    }

    if (test_transform_inverse() != 0) {
        perror("Failed to test inverse");
        test_transform_release();
        exit(-1);
    }

    test_transform_release();
    exit(0);
}
