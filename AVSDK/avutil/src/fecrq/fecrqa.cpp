
#include "fecrqm.h"
#include <string.h>

// creating A
FECRQMatrix GenCoreMatrix(const FECRQParam::Param &param, int overload)
{
    assert(overload >= 0);
    uint8_t *data = new uint8_t[param.L * (param.L + overload)];
    memset(data, 0, param.L * (param.L + overload));
    FECRQMatrix A(data, param.L + overload, param.L, true);

    // LDPC1 & Identity [B, S] & LDPC2
    for (int row = 0; row < param.S; ++row)
    {
        for (int col = 0; col < param.B; ++col)
        {
            uint16_t submtx = col / param.S;
            if ((row == (col % param.S)) || (row == (col + submtx + 1) % param.S) || (row == (col + 2 * (submtx + 1)) % param.S))
            {
                A.SetAt(row, col, 1);
            }
        }

        A.SetAt(row, param.B + row, 1);

        for (int col = 0; col < param.P; ++col)
        {
            if (col == (row % param.P) || col == ((row + 1) % param.P))
                A.SetAt(row, col + param.W, 1);
        }
    }

    // HDPC
    // make MT
    uint8_t *mtdata = new uint8_t[param.H * (param.KP + param.S)];
    FECRQMatrix MT(mtdata, param.H, param.KP + param.S, true);
    for (int row = 0; row < param.H; ++row)
    {
        int col = 0;
        for (; col < param.KP + param.S - 1; ++col)
        {
            uint32_t tmp = FECRQParam::Rand(col + 1, 6, param.H);
            if ((row == tmp) || (row == (tmp + FECRQParam::Rand(col + 1, 7, param.H - 1) + 1) % param.H))
                MT.SetAt(row, col, 1);
            else
                MT.SetAt(row, col, 0);
        }
        // last column: alpha ^^ i, as in rfc6330
        MT.SetAt(row, col, fecOCT_EXP[row]);
    }
    // make GAMMA
    uint8_t *gammadata = new uint8_t[(param.KP + param.S) * (param.KP + param.S)];
    FECRQMatrix GAMMA(gammadata, param.KP + param.S, param.KP + param.S, true);
    for (int row = 0; row < param.KP + param.S; ++row)
    {
        int col = 0;
        for (; col <= row; ++col)
            // alpha ^^ (i-j), as in rfc6330, pg24
            // rfc only says "i-j", but we could go well over oct_exp size.
            // we hope they just missed a modulus :/
            GAMMA.SetAt(row, col, fecOCT_EXP[row - col]);

        for (; col < param.KP + param.S; ++col)
            GAMMA.SetAt(row, col, 0);
    }
    
    // A[HDPC] = MT * GAMMA
    A.MulMatrix2(MT, GAMMA, param.S, param.H, param.KP + param.S);

    // Identity [H, L - H]
    for (int row = 0; row < param.H; ++row)
        A.SetAt(param.S + row, param.KP + param.S + row, 1);

    // G_ENC
    uint16_t idxes[128];
    for (int row = param.S + param.H; row < param.L; ++row)
    {
        int idxnum = FECRQParam::GetEncodedIntermediateSymbolIdx(param, row - param.S - param.H, idxes, 128);
        for (int j = 0; j < idxnum; j++)
            A.SetAt(row, idxes[j], 1);
    }

    return A;
}

