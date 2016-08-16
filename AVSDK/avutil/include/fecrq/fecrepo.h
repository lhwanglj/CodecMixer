
#ifndef _MEDIACLOUD_FECREPO_H
#define _MEDIACLOUD_FECREPO_H

#include <stdint.h>

struct fecParam {
    int K, T, KP, S, H, W, L, P, P1, U, B, J;
};

struct fecRepoInfo {
    fecParam param;
    uint8_t *matrix;
    uint32_t matrixSize;
};

#endif
