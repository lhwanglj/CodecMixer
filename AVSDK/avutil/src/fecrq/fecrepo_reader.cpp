
#include <stdint.h>
#include "fecrqm.h"
#include "../../include/fecrq/fecrepo.h"
#include "../../include/common.h"

using namespace MediaCloud::Common;

extern fecRepoInfo fecrepoinfo_2[];
// loading the A matrix directly from pre-calculated 
uint8_t *LoadFECRQCoreMatrix(uint8_t symbolSize, 
    uint16_t srcNum, uint16_t overload, FECRQParam::Param *param, bool onlyParam) 
{
    AssertMsg(symbolSize >= 2, "invalid symbol size");
    AssertMsg(srcNum >= 8 && srcNum <= 200, "invalid src number");

    fecRepoInfo &repoInfo = fecrepoinfo_2[srcNum - 8];
    if (param != nullptr) {
        param->K = repoInfo.param.K;
        param->T = symbolSize;   // only T is different
        param->KP = repoInfo.param.KP;
        param->S = repoInfo.param.S;
        param->H = repoInfo.param.H;
        param->W = repoInfo.param.W;
        param->L = repoInfo.param.L;
        param->P = repoInfo.param.P;
        param->P1 = repoInfo.param.P1;
        param->U = repoInfo.param.U;
        param->B = repoInfo.param.B;
        param->J = repoInfo.param.J;
        Assert(param->K == srcNum);  // K is the src num
        Assert(param->L == repoInfo.matrixSize); // L is the A matrix size
    }

    if (onlyParam) {
        return nullptr;
    }

    int size = repoInfo.matrixSize;
    uint8_t *matrix = new uint8_t[(size + overload) * size];
    memcpy(matrix, repoInfo.matrix, size * size);
    if (overload > 0) {
        memset(matrix + size * size, 0, overload * size);
    }
    return matrix;
}
