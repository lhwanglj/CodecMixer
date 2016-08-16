
#include "fecrqm.h"
#include <tuple>

extern uint32_t FECRQRandTableV0[256];
extern uint32_t FECRQRandTableV1[256];
extern uint32_t FECRQRandTableV2[256];
extern uint32_t FECRQRandTableV3[256];
extern uint16_t FECRQK_Padded[477];
extern uint16_t FECRQJ_K_Padded[477];
extern std::tuple<uint16_t, uint16_t, uint16_t> FECRQS_H_W[477];
extern uint32_t FECRQDegree_Distribution[31];

static bool IsPrime(const uint16_t n)
{
    // 1 as prime, don't care. Not in our scope anyway.
    // thank you stackexchange for the code
    if (n <= 3)
        return true;
    if (n % 2 == 0 || n % 3 == 0)
        return false;

    uint32_t i = 5;
    uint32_t w = 2;
    while (i * i <= n) 
    {
        if (n % i == 0)
            return false;
        i += w;
        w = 6 - w;
    }
    return true;
}

FECRQParam::Param FECRQParam::GetParam(int K, int T)
{
    Param par;
    int K_idx = 0;
    for (; K_idx < 477; ++K_idx)
    {
        if (K <= FECRQK_Padded[K_idx])
        {
            par.K = K;
            par.KP = FECRQK_Padded[K_idx];
            break;
        }
    }

    par.T = T;
    std::tie(par.S, par.H, par.W) = FECRQS_H_W[K_idx];
    par.L = par.S + par.H + par.KP;
    par.P = par.L - par.W;
    par.U = par.P - par.H;
    par.B = par.W - par.S;
    par.J = FECRQJ_K_Padded[K_idx];

    int P1 = par.P;
    if (P1 == 10)
        P1 = 11;
    else
    {
        while (!IsPrime(P1))
            ++P1;
    }
    par.P1 = P1;
    return par;
}

FECRQParam::Tuple FECRQParam::GenTuple(const Param &param, int isi)
{
    Tuple ret;
    // taken straight from RFC6330, pg 30
    // so thank them for the *beautiful* names
    // also, don't get confused with "B": this one is different,
    // and thus named "B1"
    uint32_t A = 53591 + param.J * 997;
    if (A % 2 == 0)
        ++A;

    uint32_t B1 = 10267 * (param.J + 1);
    uint32_t y = B1 + isi * A;
    uint32_t v = FECRQParam::Rand(y, 0, 1 << 20);

    ret.d = 0;
    for (uint16_t d = 1; d < 31; ++d)
    {
        if (v < FECRQDegree_Distribution[d])
        {
            ret.d = (d < (param.W - 2)) ? d : (param.W - 2);
            break;
        }
    }

    ret.a = 1 + FECRQParam::Rand(y, 1, param.W - 1);
    ret.b = FECRQParam::Rand(y, 2, param.W);
    if (ret.d < 4)
        ret.d1 = 2 + FECRQParam::Rand(isi, 3, 2);
    else
        ret.d1 = 2;

    ret.a1 = 1 + FECRQParam::Rand(isi, 4, param.P1 - 1);
    ret.b1 = FECRQParam::Rand(isi, 5, param.P1);
    return ret;
}

// returning the idx of C for encoding the ISIth repair symbols
// returning the number of idx
int FECRQParam::GetEncodedIntermediateSymbolIdx(const Param &param, int isi, uint16_t idxes[], int idxLength)
{
    int ptr = 0;
    Tuple t = GenTuple(param, isi);
    idxes[ptr++] = t.b;

    for (uint16_t j = 1; j < t.d; ++j)
    {
        t.b = (t.b + t.a) % param.W;
        idxes[ptr++] = t.b;
    }
    while (t.b1 >= (uint32_t)param.P)
        t.b1 = (t.b1 + t.a1) % param.P1;

    idxes[ptr++] = param.W + t.b1;

    for (uint16_t j = 1; j < t.d1; ++j)
    {
        t.b1 = (t.b1 + t.a1) % param.P1;
        while (t.b1 >= (uint32_t)param.P)
            t.b1 = (t.b1 + t.a1) % param.P1;
        idxes[ptr++] = param.W + t.b1;
    }

    assert(ptr <= idxLength);
    return ptr;
}

uint32_t FECRQParam::Rand(uint32_t y, uint32_t i, uint32_t m)
{
    uint32_t mod8 = 1 << 8;
    uint32_t mod16 = 1 << 16;
    uint32_t mod24 = 1 << 24;
    uint32_t x0 = (y + i) % mod8;
    uint32_t x1 = (y / mod8 + i) % mod8;
    uint32_t x2 = (y / mod16 + i) % mod8;
    uint32_t x3 = (y / mod24 + i) % mod8;
    return (uint32_t)(FECRQRandTableV0[x0] ^ FECRQRandTableV1[x1] ^ FECRQRandTableV2[x2] ^ FECRQRandTableV3[x3]) % m;
}

// determine the K
// if the K * symbolSize is larger than datelen, then the data need to be padded by zeros.
// we need to minimize the delta K * symbolSize - datalen for saving the bandwidth !!
int FECRQParam::DetermineNumOfSourceSymbols(int datalen, int symbolSize)
{
    int K = datalen / symbolSize;
    if ((datalen % symbolSize) != 0)
        K++;
    return K;
}

void FECRQMatrix::Dump() const
{
    printf("dumping matrix [%d, %d] --->\n", _rows, _columns);
    if (_rows > 0 && _columns > 0)
    {
        for (int i = 0; i < _rows; i++)
        {
            for (int j = 0; j < _columns; j++)
                printf("%4d ", _data[i * _columns + j]);
            printf("\n");
        }
    }
}

void FECRQVector::Dump() const
{
    printf("dumping vector [%d, %d] --->\n", _rows, _eleSize);
    if (_rows > 0 && _eleSize > 0)
    {
        for (int i = 0; i < _rows; i++)
        {
            for (int j = 0; j < _eleSize; j++)
                printf("%4d ", _data[i * _eleSize + j]);
            printf("\n");
        }
    }
}

void FECRQVector::Dump(int d[]) const
{
    printf("dumping vector with didx [%d, %d] --->\n", _rows, _eleSize);
    if (_rows > 0 && _eleSize > 0)
    {
        for (int i = 0; i < _rows; i++)
        {
            for (int j = 0; j < _eleSize; j++)
                printf("%4d ", _data[d[i] * _eleSize + j]);
            printf("\n");
        }
    }
}