
#include <assert.h>
#include "fast_fec.h"
#include "fast_calc.h"
#include "fecrepo_hdr.h"

using namespace fec;

bool Decoding(Matrix &A, Vector &D, const Param &param, IAllocator *allocator);
const uint8_t* GetIntermediateSymbolIdx(const Param &param, int isi, uint8_t &numOfIdxes,
    uint8_t idxesBuf[], int idxBufLength);

void Param::GetParam(int K, int T, Param &param) 
{
    assert(K >= 8 && K <= 200);
    const FecRepo::Param* repoParam = FecRepo::RepoReader::GetParam(K);
    param = *reinterpret_cast<const Param*>(repoParam);
    param.T = T;
}

const uint8_t* FastFec::GetLTSymbolData(const LTSymbols &ltSymbols, int idx)
{
    assert(idx < ltSymbols.param.L);
    int *rowIdxes = (int*)ltSymbols.symbolVec;
    const uint8_t *rowData = ltSymbols.symbolVec + CALCU_ALIGNMENT(ltSymbols.rowNum * sizeof(int), 16);
    return rowData + rowIdxes[idx] * CALCU_ALIGNMENT(ltSymbols.param.T, 16);
}

void GenLTSymbolsCommon(const Param &param, Vector &ltVec, IAllocator *allocator)
{
    const FecRepo::MatrixInfo *X = FecRepo::RepoReader::GetMatrixX(reinterpret_cast<const FecRepo::Param*>(&param));
    assert(X->rows == X->cols);
    assert(X->rows <= param.L);

    // prepare tmp rotated vec, its rowIdx is not used
    Vector rotatedVec;
    rotatedVec.rowNum = ltVec.rowSize;
    rotatedVec.rowSize = X->rows;
    rotatedVec.rowStride = X->stride;
    rotatedVec.rowIdxes = nullptr;
    rotatedVec.data = (uint8_t*)allocator->AllocFecBuffer(rotatedVec.rowStride * rotatedVec.rowNum, 16, IAllocator::PurposeTmpMatrix);

    const FecRepo::EncodeCInfo *C = FecRepo::RepoReader::GetEncodeC(reinterpret_cast<const FecRepo::Param*>(&param));
    const FecRepo::EncodeVecOpInfo *VecOps = FecRepo::RepoReader::GetEncodeVecOps(reinterpret_cast<const FecRepo::Param*>(&param));
    assert(C->L == param.L);
    assert(VecOps->L == param.L);

    for (int i = 0; i < VecOps->opsNum; i++) {
        const FecRepo::EncodeVecOp &op = VecOps->ops[i];
        switch (op.ops) {
        case FecRepo::VectorOps_SwapRows: {
            Vector_SwapRows(ltVec, op.row1, op.row2);
            break;
        }   
        case FecRepo::VectorOps_XorRows: {
            Vector_XorRows(ltVec, op.row1, op.row2, op.factor);
            break;
        }   
        case FecRepo::VectorOps_DivRow: {
            Vector_DivRow(ltVec, op.row1, op.factor);
            break;
        }   
        case FecRepo::VectorOps_MulMatrix: {
            Matrix mat;
            mat.rowNum = X->rows;
            mat.colNum = X->cols;
            mat.rowStride = X->stride;
            mat.rowIdxes = nullptr; // not used
            mat.data = const_cast<uint8_t*>(X->data); // read only
            Vector_MulMatrix(ltVec, mat, X->rows, rotatedVec);
            break;
        }
        case FecRepo::VectorOps_Reorder: {
            int tmpIdx[250]; // big enough, L = 233 for K = 200
            assert(param.L <= 250);
            Vector_Reorder(ltVec, C->data, ltVec.rowNum, tmpIdx);
            break;
        }
        default:
            assert(false);
            break;
        }
    }

    allocator->FreeFecBuffer(rotatedVec.data, IAllocator::PurposeTmpMatrix);
}

void FastFec::GenLTSymbols(const Param &param, const uint8_t* data, int paddedLen,
    IAllocator *allocator, LTSymbols &ltSymbols) 
{
    assert(allocator != nullptr); // without it we can't calculate
    assert(data != nullptr);
    assert(param.K * param.T == paddedLen);

    int idxBufLength = CALCU_ALIGNMENT(param.L * sizeof(int), 16);
    ltSymbols.param = param;
    ltSymbols.rowNum = param.L;
    ltSymbols.length = idxBufLength + CALCU_ALIGNMENT(param.T, 16) * param.L;
    ltSymbols.symbolVec = (uint8_t*)allocator->AllocFecBuffer(ltSymbols.length, 16, IAllocator::PurposeLT);

    Vector ltVec;
    ltVec.rowNum = param.L;
    ltVec.rowSize = param.T;
    ltVec.rowStride = CALCU_ALIGNMENT(param.T, 16);
    ltVec.rowIdxes = (int*)ltSymbols.symbolVec;
    ltVec.data = ltSymbols.symbolVec + idxBufLength;

    // initial row index
    for (int i = 0; i < param.L; i++) {
        ltVec.rowIdxes[i] = i;
    }

    // set data
    memset(ltVec.data, 0, (param.S + param.H) * ltVec.rowStride);
    uint8_t *srcdata = ltVec.data + (param.S + param.H) * ltVec.rowStride;
    for (int i = 0; i < param.K; i++) {
        memcpy(srcdata, data, param.T);
        if (ltVec.rowStride > param.T) {
            memset(srcdata + param.T, 0, ltVec.rowStride - param.T);
        }
        srcdata += ltVec.rowStride;
        data += param.T;
    }
    if (param.KP > param.K) {
        memset(srcdata, 0, (param.KP - param.K) * ltVec.rowStride);
    }
    
    GenLTSymbolsCommon(param, ltVec, allocator);
}

void FastFec::GenLTSymbols(const Param &param, const uint8_t* srcSymData[], int srcSymNum, 
    IAllocator *allocator, LTSymbols &ltSymbols)
{
    assert(allocator != nullptr); // without it we can't calculate
    assert(srcSymData != nullptr);
    assert(param.K == srcSymNum);

    int idxBufLength = CALCU_ALIGNMENT(param.L * sizeof(int), 16);
    ltSymbols.param = param;
    ltSymbols.rowNum = param.L;
    ltSymbols.length = idxBufLength + CALCU_ALIGNMENT(param.T, 16) * param.L;
    ltSymbols.symbolVec = (uint8_t*)allocator->AllocFecBuffer(ltSymbols.length, 16, IAllocator::PurposeLT);

    Vector ltVec;
    ltVec.rowNum = param.L;
    ltVec.rowSize = param.T;
    ltVec.rowStride = CALCU_ALIGNMENT(param.T, 16);
    ltVec.rowIdxes = (int*)ltSymbols.symbolVec;
    ltVec.data = ltSymbols.symbolVec + idxBufLength;

    // initial row index
    for (int i = 0; i < param.L; i++) {
        ltVec.rowIdxes[i] = i;
    }

    // set data
    memset(ltVec.data, 0, (param.S + param.H) * ltVec.rowStride);
    uint8_t *srcdata = ltVec.data + (param.S + param.H) * ltVec.rowStride;
    for (int ptr = 0; ptr < param.K; ptr++) {
        assert(srcSymData[ptr] != nullptr);
        memcpy(srcdata, srcSymData[ptr], param.T);
        if (ltVec.rowStride > param.T) {
            memset(srcdata + param.T, 0, ltVec.rowStride - param.T);
        }
        srcdata += ltVec.rowStride;
    }
    
    if (param.KP > param.K) {
        memset(srcdata, 0, (param.KP - param.K) * ltVec.rowStride);
    }
    
    GenLTSymbolsCommon(param, ltVec, allocator);
}

void ConstructDecodingMatrixA(const FecRepo::MatrixInfo *repoA, int deltaSymNum, Matrix &A, IAllocator *allocator)
{
    int rowNumOfA = repoA->rows + deltaSymNum;
    uint8_t *bufferOfA = (uint8_t*)allocator->AllocFecBuffer(
        CALCU_ALIGNMENT(rowNumOfA * sizeof(int), 16) + // row idxes
        repoA->stride * rowNumOfA, // row data
        16, IAllocator::PurposeTmpMatrix);

    A.rowNum = rowNumOfA; // param.L + addtional repair symbols
    A.colNum = repoA->cols;
    A.rowStride = repoA->stride;
    A.rowIdxes = (int*)bufferOfA;
    for (int i = 0; i < rowNumOfA; i++) {
        A.rowIdxes[i] = i;
    }
    
    A.data = bufferOfA + CALCU_ALIGNMENT(rowNumOfA * sizeof(int), 16);
    memcpy(A.data, repoA->data, repoA->stride * repoA->rows); // copy in the original A
    if (rowNumOfA > repoA->rows) {
        memset(A.data + repoA->stride * repoA->rows, 0, repoA->stride * (rowNumOfA - repoA->rows)); // zero additional part
    }
}

void ConstructDecodingLTSymbols(const Param &param, int deltaSymNum, LTSymbols &ltSymbols, Vector &ltVec, IAllocator *allocator)
{
    // D is the input symbols before decoding, and the output of intermediate symbols after decoding
    // after decoding, it has L LT symbols + unuseless symbols at tail
    int rowNumOfA = param.L + deltaSymNum;
    ltSymbols.param = param;
    ltSymbols.rowNum = rowNumOfA;
    ltSymbols.length = CALCU_ALIGNMENT(rowNumOfA * sizeof(int), 16) + CALCU_ALIGNMENT(param.T, 16) * rowNumOfA;
    ltSymbols.symbolVec = (uint8_t*)allocator->AllocFecBuffer(ltSymbols.length, 16, IAllocator::PurposeLT);

    ltVec.rowNum = rowNumOfA;
    ltVec.rowSize = param.T;
    ltVec.rowStride = CALCU_ALIGNMENT(param.T, 16);
    ltVec.rowIdxes = (int*)ltSymbols.symbolVec;
    ltVec.data = ltSymbols.symbolVec + CALCU_ALIGNMENT(rowNumOfA * sizeof(int), 16);
    for (int i = 0; i < rowNumOfA; i++) {
        ltVec.rowIdxes[i] = i;
    }
    
    // data is not initialized
}

bool FastFec::TryDecode(const Param &param, const uint8_t* symbols[], int maxESI, int validSymbolNum,
    IAllocator *allocator, LTSymbols &ltSymbols)
{
    assert(allocator != nullptr);
    assert(validSymbolNum >= param.K && validSymbolNum <= maxESI);
    assert(maxESI >= param.K);

    const FecRepo::MatrixInfo *repoA = 
        FecRepo::RepoReader::GetMatrixA(reinterpret_cast<const FecRepo::Param*>(&param));
    assert(repoA->rows == param.L);
    assert(repoA->cols == param.L);

    int deltaSymNum = validSymbolNum - param.K;
    
    // construct A
    Matrix A;
    ConstructDecodingMatrixA(repoA, deltaSymNum, A, allocator);
    
    Vector ltVec;
    ConstructDecodingLTSymbols(param, deltaSymNum, ltSymbols, ltVec, allocator);
    
    // initialize lt symbols and revise the matrix A with repair symbols
    // set A and D's data from symbols
    memset(ltVec.data, 0, (param.S + param.H) * ltVec.rowStride);
    
    uint8_t idxes[256];
    uint8_t numOfIdxes = 0;
    const uint8_t *ptupleIdxes = nullptr;

    // copy the src symbols into D
    int setSymbolCnt = 0;
    int repairEsi = param.K;

    // maxESI is also the symbol num, and i is the esi
    // first set those valid src symbols into lt symbols
    for (int i = 0; i < param.K; i++) {
        const uint8_t *srcptr = symbols[i];
        int esi = i;
        if (srcptr == nullptr) {
            // src symbol is not valid, so we use repair symbols instead
            for (; repairEsi < maxESI; repairEsi++) {
                if (symbols[repairEsi] != nullptr) {
                    esi = repairEsi++;  // pointing to the next
                    break;
                }
            }
            assert(esi >= param.K && esi < maxESI);
            srcptr = symbols[esi];
        }
        
        uint8_t *rowdata = ltVec.data + (param.S + param.H + i) * ltVec.rowStride;
        memcpy(rowdata, srcptr, param.T);
        if (ltVec.rowStride > param.T) {
            memset(rowdata + param.T, 0, ltVec.rowStride - param.T);
        }

        if (esi >= param.K) {
            // this is a repair symbol, we need replace Matrix A row
            uint8_t *rowOfA = A.data + A.rowStride * (i + param.S + param.H);
            ptupleIdxes = GetIntermediateSymbolIdx(param, param.ESIToISI(esi), numOfIdxes, idxes, sizeof(idxes));
            memset(rowOfA, 0, A.rowStride);
            for (uint8_t c = 0; c < numOfIdxes; c++) {
                rowOfA[ptupleIdxes[c]] = 1;
            }
        }
        
        setSymbolCnt++;
    }
    
    assert(setSymbolCnt == param.K);
    
    // zero the vector rows [K - KP]
    if (param.KP > param.K) {
        memset(ltVec.data + (param.S + param.H + param.K) * ltVec.rowStride, 0, (param.KP - param.K) * ltVec.rowStride);
    }
    
    // set additonal repair symbols after the L lt symbols
    int addi = param.L;
    for (; repairEsi < maxESI; repairEsi++) {
        if (symbols[repairEsi] == nullptr) {
            continue;
        }
        
        assert(addi < ltVec.rowNum);
        
        uint8_t *rowdata = ltVec.data + ltVec.rowStride * addi;
        memcpy(rowdata, symbols[repairEsi], param.T);
        if (ltVec.rowStride > param.T) {
            memset(rowdata + param.T, 0, ltVec.rowStride - param.T);
        }

        uint8_t *rowOfA = A.data + A.rowStride * addi;
        ptupleIdxes = GetIntermediateSymbolIdx(param, param.ESIToISI(repairEsi), numOfIdxes, idxes, sizeof(idxes));
        // this row A already zeroed when constructing matrix A
        for (uint8_t c = 0; c < numOfIdxes; c++) {
            rowOfA[ptupleIdxes[c]] = 1;
        }
        
        addi++;
        setSymbolCnt++;
    }
    
    assert(setSymbolCnt == validSymbolNum);

    bool suc = Decoding(A, ltVec, param, allocator);
    if (!suc) {
        allocator->FreeFecBuffer(ltSymbols.symbolVec, IAllocator::PurposeLT);
        ltSymbols.symbolVec = nullptr;
    }
    allocator->FreeFecBuffer(A.rowIdxes, IAllocator::PurposeTmpMatrix);
    return suc;
}

bool FastFec::TryDecode(const Param &param, const Symbol *symbols, int numOfSymbol,
    IAllocator *allocator, LTSymbols &ltSymbols)
{
    assert(allocator != nullptr);
    assert(numOfSymbol >= param.K);

    const FecRepo::MatrixInfo *repoA = 
        FecRepo::RepoReader::GetMatrixA(reinterpret_cast<const FecRepo::Param*>(&param));
    assert(repoA->rows == param.L);
    assert(repoA->cols == param.L);

    // construct A
    Matrix A;
    ConstructDecodingMatrixA(repoA, numOfSymbol - param.K, A, allocator);
    
    // construct D
    Vector ltVec;
    ConstructDecodingLTSymbols(param, numOfSymbol - param.K, ltSymbols, ltVec, allocator);
    
    // set A and D's data from symbols
    memset(ltVec.data, 0, (param.S + param.H) * ltVec.rowStride);
    
    uint8_t idxes[256];
    uint8_t numOfIdxes = 0;
    const uint8_t *ptupleIdxes = nullptr;

    // copy the src symbols into D
    int lastEsi = -1;
    int repairBegin = -1;
    int srcSymCnt = 0;
    uint8_t srcSymFlag[300 / 8] = { 0 }; // big enough
    for (int i = 0; i < numOfSymbol; i++) {
        if (symbols[i].data == nullptr) {
            continue;
        }
        
        assert(symbols[i].esi > lastEsi); // assert the esi is increasing purely
        lastEsi = symbols[i].esi;
        if (lastEsi >= param.K) {
            repairBegin = i;
            break;
        }

        uint8_t *rowdata = ltVec.data + (param.S + param.H + lastEsi) * ltVec.rowStride;
        memcpy(rowdata, symbols[i].data, param.T);
        if (ltVec.rowStride > param.T) {
            memset(rowdata + param.T, 0, ltVec.rowStride - param.T);
        }

        srcSymCnt++;
        srcSymFlag[lastEsi / 8] |= 1 << (lastEsi % 8);
    }

    if (repairBegin == -1) {
        // all input symbols are src symbols, there is no repair symbols
        assert(numOfSymbol == param.K && srcSymCnt == param.K);
    }
    else {
        assert(srcSymCnt <= param.K && (param.K - srcSymCnt) <= (numOfSymbol - repairBegin));
        if (srcSymCnt < param.K) {
            // use the repair symbols to replace the src symbols, and also in the matrix A
            for (int i = 0; i < param.K; i++) {
                if ((srcSymFlag[i / 8] & (1 << (i % 8))) == 0) {
                    // src symbol missing
                    uint8_t *rowdata = ltVec.data + (param.S + param.H + i) * ltVec.rowStride;
                    memcpy(rowdata, symbols[repairBegin].data, param.T);
                    if (ltVec.rowStride > param.T) {
                        memset(rowdata + param.T, 0, ltVec.rowStride - param.T);
                    }

                    // replace the matrix A row
                    uint8_t *rowOfA = A.data + A.rowStride * (i + param.S + param.H);
                    ptupleIdxes = GetIntermediateSymbolIdx(param, param.ESIToISI(symbols[repairBegin].esi), numOfIdxes, idxes, 256);
                    memset(rowOfA, 0, A.rowStride);
                    for (uint8_t c = 0; c < numOfIdxes; c++) {
                        rowOfA[ptupleIdxes[c]] = 1;
                    }

                    ++repairBegin;
                    if (++srcSymCnt >= param.K) {
                        break;
                    }
                }
            }
        }
        else {
            // Warning: since the all src symbols are existing, the LTSymbols should be calculated by GenLTSymbols
        }
    }

    // zero the vector rows [K - KP]
    if (param.KP > param.K) {
        memset(ltVec.data + (param.S + param.H + param.K) * ltVec.rowStride, 0, (param.KP - param.K) * ltVec.rowStride);
    }

    // set in those additional symbols
    if (repairBegin != -1 && repairBegin < numOfSymbol) {
        for (int addi = param.L; repairBegin < numOfSymbol; repairBegin++, addi++) {
            uint8_t *rowdata = ltVec.data + ltVec.rowStride * addi;
            memcpy(rowdata, symbols[repairBegin].data, param.T);
            if (ltVec.rowStride > param.T) {
                memset(rowdata + param.T, 0, ltVec.rowStride - param.T);
            }

            uint8_t *rowOfA = A.data + A.rowStride * addi;
            ptupleIdxes = GetIntermediateSymbolIdx(param, param.ESIToISI(symbols[repairBegin].esi), numOfIdxes, idxes, 256);
            for (uint8_t c = 0; c < numOfIdxes; c++) {
                rowOfA[ptupleIdxes[c]] = 1;
            }

            ++srcSymCnt;
        }
    }

    assert(srcSymCnt == numOfSymbol);

    bool suc = Decoding(A, ltVec, param, allocator);
    if (!suc) {
        allocator->FreeFecBuffer(ltSymbols.symbolVec, IAllocator::PurposeLT);
        ltSymbols.symbolVec = nullptr;
    }
    allocator->FreeFecBuffer(A.rowIdxes, IAllocator::PurposeTmpMatrix);
    return suc;
}

/*
* gen symbol from intermediate symbols, data length is larger than param.T
* data should be aligned to 16bytes
*/
void FastFec::RecoverSymbol(const LTSymbols &ltSymbols, int esi, 
    uint8_t *data, int length)
{
    const Param &param = ltSymbols.param;
    int stride = CALCU_ALIGNMENT(param.T, 16);
    assert(ltSymbols.length >= (stride * ltSymbols.rowNum + CALCU_ALIGNMENT(ltSymbols.rowNum * sizeof(int), 16)));
    assert(ltSymbols.symbolVec != nullptr && ((uint64_t)ltSymbols.symbolVec & 0xF) == 0);
    assert(data != nullptr && ((uint64_t)data & 0xF) == 0);
    assert(length >= stride);

    uint8_t idxes[256];
    uint8_t numOfIdxes = 0;
    const uint8_t *ptupleIdxes = GetIntermediateSymbolIdx(param, param.ESIToISI(esi), numOfIdxes, idxes, 256);

    // xor out the symbols from lt symbols
    int *rowIdxes = (int*)ltSymbols.symbolVec;
    const uint8_t *rowData = ltSymbols.symbolVec + CALCU_ALIGNMENT(ltSymbols.rowNum * sizeof(int), 16);

    memcpy(data, rowData + stride * rowIdxes[ptupleIdxes[0]], stride); // copy the whole stride which padded with zeros
    for (uint8_t c = 1; c < numOfIdxes; c++) {
        XorRows(data, rowData + stride * rowIdxes[ptupleIdxes[c]], 1, stride);
    }
}

extern uint32_t FECRQRandTableV0[256];
extern uint32_t FECRQRandTableV1[256];
extern uint32_t FECRQRandTableV2[256];
extern uint32_t FECRQRandTableV3[256];
extern uint32_t FECRQDegree_Distribution[31];

static uint32_t FecRand(uint32_t y, uint32_t i, uint32_t m)
{
    uint32_t mod8 = 1 << 8;
    uint32_t mod16 = 1 << 16;
    uint32_t mod24 = 1 << 24;
    uint32_t x0 = (y + i) % mod8;
    uint32_t x1 = (y / mod8 + i) % mod8;
    uint32_t x2 = (y / mod16 + i) % mod8;
    uint32_t x3 = (y / mod24 + i) % mod8;
    return (uint32_t) (FECRQRandTableV0[x0] ^ FECRQRandTableV1[x1] ^ FECRQRandTableV2[x2] ^ FECRQRandTableV3[x3]) % m;
}

static void GenTuple(const Param &param, int isi, FecRepo::Tuple &ret)
{
    // taken straight from RFC6330, pg 30
    // so thank them for the *beautiful* names
    // also, don't get confused with "B": this one is different,
    // and thus named "B1"
    uint32_t A = 53591 + param.J * 997;
    if (A % 2 == 0) {
        ++A;
    }

    uint32_t B1 = 10267 * (param.J + 1);
    uint32_t y = B1 + isi * A;
    uint32_t v = FecRand(y, 0, 1 << 20);

    ret.d = 0;
    for (uint16_t d = 1; d < 31; ++d) {
        if (v < FECRQDegree_Distribution[d]) {
            ret.d = (d < (param.W - 2)) ? d : (param.W - 2);
            break;
        }
    }

    ret.a = 1 + FecRand(y, 1, param.W - 1);
    ret.b = FecRand(y, 2, param.W);
    if (ret.d < 4) {
        ret.d1 = 2 + FecRand(isi, 3, 2);
    }
    else {
        ret.d1 = 2;
    }

    ret.a1 = 1 + FecRand(isi, 4, param.P1 - 1);
    ret.b1 = FecRand(isi, 5, param.P1);
}

// tuple content will be modified
// we only support at most 256 intermediate symbols
static int GetIntermediateSymbolIdxFromTuple(const Param &param, FecRepo::Tuple &t, 
    int isi, uint8_t idxes[], int idxLength)
{
    int ptr = 0;
    idxes[ptr++] = t.b;

    for (uint16_t j = 1; j < t.d; ++j) {
        t.b = (t.b + t.a) % param.W;
        idxes[ptr++] = t.b;
    }

    while (t.b1 >= (uint32_t)param.P) {
        t.b1 = (t.b1 + t.a1) % param.P1;
    }

    idxes[ptr++] = param.W + t.b1;

    for (uint16_t j = 1; j < t.d1; ++j) {
        t.b1 = (t.b1 + t.a1) % param.P1;
        while (t.b1 >= (uint32_t)param.P) {
            t.b1 = (t.b1 + t.a1) % param.P1;
        }
        idxes[ptr++] = param.W + t.b1;
    }

    assert(ptr <= idxLength);
    return ptr;
}

/*
* returning from cache or generate the idxes on the fly
*/
const uint8_t* GetIntermediateSymbolIdx(const Param &param, int isi, uint8_t &numOfIdxes,
    uint8_t idxesBuf[], int idxBufLength)
{
    const uint8_t *ptupleIdxes = nullptr;
    numOfIdxes = 0;

    FecRepo::Tuple tuple;
    const FecRepo::Tuple *pRepoTuple =
        FecRepo::RepoReader::GetTuple(reinterpret_cast<const FecRepo::Param*>(&param), isi, ptupleIdxes, numOfIdxes);
    if (pRepoTuple == nullptr) {
        // the isi is too large for cache, we have to calculate it on the fly
        GenTuple(param, isi, tuple);
        ptupleIdxes = idxesBuf;
        numOfIdxes = GetIntermediateSymbolIdxFromTuple(param, tuple, isi, idxesBuf, idxBufLength);
    }

    assert(numOfIdxes > 0);
    assert(ptupleIdxes != nullptr);
    return ptupleIdxes;
}
