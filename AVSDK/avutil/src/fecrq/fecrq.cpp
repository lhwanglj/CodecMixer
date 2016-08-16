
#include "fecrqm.h"
#include "fecrq.h"
#include <assert.h>
#include <vector>
#include <stdint.h>


//FECRQMatrix GenCoreMatrix(const FECRQParam::Param &param, int overload);
FECRQVector FECRQDecoding(FECRQMatrix &A, FECRQVector &D, const FECRQParam::Param &param);
uint8_t *LoadFECRQCoreMatrix(uint8_t symbolSize,
    uint16_t srcNum, uint16_t overload, FECRQParam::Param *param, bool onlyParam);

// used to create the repair symbol or recover the source symbols
void FECRQMakeSymbol(
    const FECRQParam::Param &param, int isi, FECRQVector &C, uint8_t data[])
{
    FECRQParam::Tuple tuple = FECRQParam::GenTuple(param, isi);
    
    memcpy(data, C.GetRow(tuple.b), param.T);
    FECRQVector tmp(data, 1, param.T, false);

    for (unsigned int j = 1; j < tuple.d; j++)
    {
        tuple.b = (tuple.b + tuple.a) % param.W;
        tmp.AddRow(0, C, tuple.b);
    }

    while (tuple.b1 >= (uint32_t)param.P)
        tuple.b1 = (tuple.b1 + tuple.a1) % param.P1;

    tmp.AddRow(0, C, param.W + tuple.b1);

    for (unsigned int j = 1; j < tuple.d1; j++)
    {
        tuple.b1 = (tuple.b1 + tuple.a1) % param.P1;
        while (tuple.b1 >= (uint32_t)param.P)
            tuple.b1 = (tuple.b1 + tuple.a1) % param.P1;

        tmp.AddRow(0, C, param.W + tuple.b1);
    }
}

FECRQEncoder::FECRQEncoder(
    const uint8_t data[], int length, int symbolSize) 
{
    int K = FECRQParam::DetermineNumOfSourceSymbols(length, symbolSize);
    uint8_t *coreMatrix = LoadFECRQCoreMatrix(symbolSize, K, 0, &_param, false);
    _coreMatrix.SetData(coreMatrix, _param.L, _param.L, true);
    
    // gen D
    // prepare D, and D will be changed during decoding process
    // so have to allocate new buffer for that
    uint16_t paddingSize = _param.K * _param.T - length;
    _dbuf = new uint8_t[_param.L * _param.T];
    memset(_dbuf, 0, (_param.S + _param.H) * _param.T);
    // copy in the payload
    memcpy(_dbuf + (_param.S + _param.H) * _param.T, data, length);
    if (paddingSize > 0) {
        memset(_dbuf + (_param.S + _param.H) * _param.T + length, 0, paddingSize);
    }

    if (_param.KP > _param.K) {
        memset(_dbuf + (_param.S + _param.H + _param.K) * _param.T, 0, (_param.KP - _param.K) * _param.T);
    }

    _length = length;
//    printf("fecencoder param: K = %d, K' = %d, T = %d, S = %d, H = %d, L = %d, P = %d, P1 = %d, Size = %d\n",
//        _param.K, _param.KP, _param.T, _param.S, _param.H, _param.L, _param.P, _param.P1, _param.K * _param.T);
}

// the symbols input buffer has the last src symbols padding 0s.
FECRQEncoder::FECRQEncoder(
    const uint8_t* symbols[], int payloadLength, int symbolSize) {
    
    int K = FECRQParam::DetermineNumOfSourceSymbols(payloadLength, symbolSize);
    uint8_t *coreMatrix = LoadFECRQCoreMatrix(symbolSize, K, 0, &_param, false);
    _coreMatrix.SetData(coreMatrix, _param.L, _param.L, true);

    // gen D
    _dbuf = new uint8_t[_param.L * _param.T];
    memset(_dbuf, 0, (_param.S + _param.H) * _param.T);
    // copy in the payload
    uint8_t *p = _dbuf + (_param.S + _param.H) * _param.T;
    for (int i = 0; i < _param.K; i++) {
        memcpy(p, symbols[i], _param.T);
        p += _param.T;
    }

    if (_param.KP > _param.K) {
        memset(_dbuf + (_param.S + _param.H + _param.K) * _param.T, 0, (_param.KP - _param.K) * _param.T);
    }

    _length = payloadLength;
//  printf("fecencoder param: K = %d, K' = %d, T = %d, S = %d, H = %d, L = %d, P = %d, P1 = %d, Size = %d\n",
//      _param.K, _param.KP, _param.T, _param.S, _param.H, _param.L, _param.P, _param.P1, _param.K * _param.T);
}

FECRQEncoder::FECRQEncoder(
    const FECRQParam::Param &param, FECRQVector &cSymbols, int length) {
    _dbuf = nullptr;
    _param = param;
    _cSymbols = cSymbols;
    _length = length;
//  printf("fecencoder param: K = %d, K' = %d, T = %d, S = %d, H = %d, L = %d, P = %d, P1 = %d, Size = %d\n",
//      _param.K, _param.KP, _param.T, _param.S, _param.H, _param.L, _param.P, _param.P1, _param.K * _param.T);
}

FECRQEncoder::~FECRQEncoder()
{
    delete[] _dbuf;
}

void FECRQEncoder::GetEncodedSymbols(int esi, uint8_t data[])
{
    if (_cSymbols.RawData() == nullptr)
        GenerateCSymbols();

    int isi = FECRQParam::ESIToISI(_param, esi);
    FECRQMakeSymbol(_param, isi, _cSymbols, data);
}

void FECRQEncoder::ForceEncoding()
{
    if (_cSymbols.RawData() == nullptr)
        GenerateCSymbols();
}

void FECRQEncoder::GenerateCSymbols()
{
    FECRQVector D (_dbuf, _param.L, _param.T, true);
    _dbuf = nullptr;

    _cSymbols = FECRQDecoding(_coreMatrix, D, _param);

    assert(_cSymbols.RawData() != nullptr);
    assert(_cSymbols.Rows() == _param.L);

    // once we get isymbols, we don't need to A matrix
    _coreMatrix.SetData(nullptr, 0, 0, false);
}

////////////////// FECRQDecoder
FECRQDecoder::FECRQDecoder(
    int dataLength, int symbolSize, int numOfSourceSymbols, int overload)
{
    assert(overload >= 0);
    LoadFECRQCoreMatrix(symbolSize, numOfSourceSymbols, overload, &_param, true);

    _data = new uint8_t[_param.K * _param.T];
    _overload = overload;
    _sourceSymbolNum = 0;
    _dataLength = dataLength;

    int maskSize = _param.K / 8 + 1;
    _sourceSymbolMask = new uint8_t[maskSize];
    memset(_sourceSymbolMask, 0, maskSize);
}

FECRQDecoder::~FECRQDecoder()
{
    delete[] _data;
    delete[] _sourceSymbolMask;
    for (auto &pair : _repairSymbols)
        delete[] pair.second;
}

// the data length >= symbolSize
// returning true if the decoding successfully completed
bool FECRQDecoder::AddEncodedSymbols(int esi, const uint8_t data[])
{
    assert(esi >= 0);
    assert(data != nullptr);

    // source symbols
    if (_sourceSymbolNum >= _param.K)
        return true;    // already decoded successfully

    bool newSymbol = false;
    if (esi < _param.K)
    {
        if ((_sourceSymbolMask[esi / 8] & (1 << (esi % 8))) == 0)
        {
            newSymbol = true;
            memcpy(_data + esi * _param.T, data, _param.T);

            _sourceSymbolMask[esi / 8] |= 1 << (esi % 8);
            _sourceSymbolNum++;
        }
    }
    else
    {
        for (auto &pair : _repairSymbols)
        {
            if (pair.first == esi)
                return false;   // dup
        }

        // the repairSymbol doesn't need ordered
        newSymbol = true;
        uint8_t *newdata = new uint8_t[_param.T];
        memcpy(newdata, data, _param.T);
        _repairSymbols.push_back(std::make_pair(esi, newdata));
    }

    if (newSymbol)
    {
        // check again
        if (_sourceSymbolNum >= _param.K)
            return true;

        // check if we have enough symbols to decode
        if (_sourceSymbolNum + _repairSymbols.size() >= _param.K + _overload)
        {
            TryDecoding();
            return _sourceSymbolNum >= _param.K;
        }
    }

    // no enough symbol to decoding, or decoding fails
    return false;
}

// esi < numOfSourceSymbols
// returning buffer pointer directly
const uint8_t* FECRQDecoder::GetSourceSymbols(int esi) const
{
    assert(esi < _param.K);
    if ((_sourceSymbolMask[esi / 8] & (1 << (esi % 8))) != 0)
        return _data + esi * _param.T;
    return nullptr;
}

void FECRQDecoder::TryDecoding()
{
    int overhead = (int)(_repairSymbols.size() - (_param.K - _sourceSymbolNum));
    uint8_t *aMatrix = LoadFECRQCoreMatrix(_param.T, _param.K, overhead, nullptr, false);
    FECRQMatrix A (aMatrix, _param.L + overhead, _param.L, true);

    // replace the missing source symbols with the repair symbols
    uint16_t idxes[256]; // it is big enough for all cases
    int repairIdx = 0;
    for (int i = 0; i < _param.K; i++)
    {
        if ((_sourceSymbolMask[i / 8] & (1 << (i % 8))) == 0)
        {
            // missing one
            A.ZeroRow(_param.S + _param.H + i);

            int num = FECRQParam::GetEncodedIntermediateSymbolIdx(_param, 
                FECRQParam::ESIToISI(_param, _repairSymbols[repairIdx].first), idxes, 256);
            for (int j = 0; j < num; j++)
                A.SetAt(_param.S + _param.H + i, idxes[j], 1);

            repairIdx++;
        }
    }

    int overheadIdx = 0;
    for (; repairIdx < _repairSymbols.size(); repairIdx++)
    {
        // don't need zero the rows, they already are
        // append with repair symbol
        int num = FECRQParam::GetEncodedIntermediateSymbolIdx(_param,
            FECRQParam::ESIToISI(_param, _repairSymbols[repairIdx].first), idxes, 256);
        for (int j = 0; j < num; j++)
            A.SetAt(_param.L + overheadIdx, idxes[j], 1);

        overheadIdx++;
    }
    assert(overheadIdx == overhead);

    // generate D vector
    uint8_t *dbuf = new uint8_t[(_param.L + overhead) * _param.T];
    // zero S and H rows
    memset(dbuf, 0, (_param.S + _param.H) * _param.T);
    // copy in the recved source symbols
    memcpy(dbuf + (_param.S + _param.H) * _param.T, _data, _param.K * _param.T);
    // zero K' - K rows
    if (_param.KP > _param.K)
        memset(dbuf + (_param.S + _param.H + _param.K) * _param.T, 0, (_param.KP - _param.K) * _param.T);
    // replace the missing source symbols with repaired
    repairIdx = 0;
    for (int i = 0; i < _param.K; i++)
    {
        if ((_sourceSymbolMask[i / 8] & (1 << (i % 8))) == 0)
        {
            // missing one
            memcpy(dbuf + (_param.S + _param.H + i) * _param.T, _repairSymbols[repairIdx].second, _param.T);
            repairIdx++;
        }
    }
    // copy in the repaired symbols
    overheadIdx = 0;
    for (; repairIdx < _repairSymbols.size(); repairIdx++)
    {
        memcpy(dbuf + (_param.L + overheadIdx) * _param.T, _repairSymbols[repairIdx].second, _param.T);
        overheadIdx++;
    }

    FECRQVector D(dbuf, _param.L + overhead, _param.T, true);
    FECRQVector C = FECRQDecoding(A, D, _param);
    if (C.RawData() == nullptr)
        return; // failure

    // recover the missing sources
    for (int i = 0; i < _param.K; i++)
    {
        if ((_sourceSymbolMask[i / 8] & (1 << (i % 8))) == 0)
        {
            FECRQMakeSymbol(_param, i, C, _data + i * _param.T);
            _sourceSymbolMask[i / 8] |= 1 << (i % 8);
            _sourceSymbolNum++;
        }
    }
    assert(_sourceSymbolNum == _param.K);
}

// for server decoding
// symbols is the pointer to symbol data from esi is [0 - esiEnd)
// no symbol have nullprt in symbols[], symbolNum is the pointer number in symbols[]
FECRQEncoder* FECRQDecoder::Decode(int dataLength, int symbolSize,
    const uint8_t* symbols[], uint16_t symbolNum, uint16_t esiEnd) 
{
    int K = FECRQParam::DetermineNumOfSourceSymbols(dataLength, symbolSize);
    assert(K < symbolNum && esiEnd > K);

    int overhead = symbolNum - K;
    FECRQParam::Param param;
    uint8_t *coreMatrix = LoadFECRQCoreMatrix(symbolSize, K, overhead, &param, false);
    FECRQMatrix A(coreMatrix, param.L + overhead, param.L, true);

    // prepare D
    uint16_t idxes[256]; // it is big enough for all cases
    uint8_t *dbuf = new uint8_t[(param.L + overhead) * param.T];
    // zero S and H rows
    memset(dbuf, 0, (param.S + param.H) * param.T);

    uint8_t *psrcbuf = dbuf + (param.S + param.H) * param.T;
    int repairIdx = K;
    for (int i = 0; i < K; i++) {
        if (symbols[i] == nullptr) {
            // missing one src symbols, use one repair symbol
            const uint8_t *repairSym = nullptr;
            uint16_t repairESI = 0xFFFF;
            while (repairIdx < esiEnd) {
                repairSym = symbols[repairIdx];
                if (repairSym != nullptr) {
                    repairESI = repairIdx;
                    repairIdx++;
                    break;
                }
                repairIdx++;
            }
            assert(repairSym != nullptr); // symbolNum > K qurantee this assert

            // revise A for the repair symbol
            A.ZeroRow(param.S + param.H + i);
            int num = FECRQParam::GetEncodedIntermediateSymbolIdx(param,
                FECRQParam::ESIToISI(param, repairESI), idxes, 256);
            for (int j = 0; j < num; j++) {
                A.SetAt(param.S + param.H + i, idxes[j], 1);
            }
            // copy into repair symbol
            memcpy(psrcbuf + i * param.T, repairSym, param.T);
        }
        else {
            memcpy(psrcbuf + i * param.T, symbols[i], param.T);
        }
    }

    // zero K' - K rows
    if (param.KP > param.K) {
        memset(psrcbuf + K * param.T, 0, (param.KP - param.K) * param.T);
    }

    // there must have additional symbols to fill the overhead
    int overheadIdx = 0;
    assert(repairIdx < esiEnd);
    while (repairIdx < esiEnd) {
        if (symbols[repairIdx] == nullptr) {
            repairIdx++;
            continue;
        }

        // don't need zero the overhead rows, they are zeroed when loading A
        int num = FECRQParam::GetEncodedIntermediateSymbolIdx(param,
            FECRQParam::ESIToISI(param, repairIdx), idxes, 256);
        for (int j = 0; j < num; j++) {
            A.SetAt(param.L + overheadIdx, idxes[j], 1);
        }

        memcpy(dbuf + (param.L + overheadIdx) * param.T, symbols[repairIdx], param.T);
        overheadIdx++;
        repairIdx++;
    }
    assert(overheadIdx == overhead);

    // decoding
    FECRQVector D (dbuf, param.L + overhead, param.T, true);
    FECRQVector C = FECRQDecoding(A, D, param);
    if (C.RawData() == nullptr) {
        return nullptr; // failure
    }
    return new FECRQEncoder(param, C, dataLength);
}

// this is another way to decode for client
// symbols is a buffer that has format of symbolNum pairs of | 2byte esi | symbol data |
// the last src symbol has zero padding !!
uint8_t* FECRQDecoder::Decode(int dataLength, int symbolSize,
    const uint8_t* symbols, uint16_t symbolNum) 
{
    int K = FECRQParam::DetermineNumOfSourceSymbols(dataLength, symbolSize);
    assert(K < symbolNum);

    int overhead = symbolNum - K;
    FECRQParam::Param param;
    uint8_t *coreMatrix = LoadFECRQCoreMatrix(symbolSize, K, overhead, &param, false);
    FECRQMatrix A(coreMatrix, param.L + overhead, param.L, true);

    // prepare D
    uint16_t idxes[256]; // it is big enough for all cases
    uint8_t *dbuf = new uint8_t[(param.L + overhead) * param.T];
    // zero S and H rows
    memset(dbuf, 0, (param.S + param.H) * param.T);
    // prepare src symbols
    uint8_t *psrcbuf = dbuf + (param.S + param.H) * param.T;

    const uint8_t** repairSymbols = new const uint8_t*[symbolNum];
    uint16_t repairCnt = 0;

    // first round, fill all good source symbols
    uint8_t srcmask[40] = { 0 };
    assert(K < 40 * 8);
    uint16_t srcCnt = 0;
    const uint8_t *psymbol = symbols;
    for (uint16_t i = 0; i < symbolNum; i++) {
        uint16_t esi = *(uint16_t*)psymbol;
        if (esi < K) {
            // a src symbol
            assert((srcmask[esi / 8] & (1 << (esi % 8))) == 0);
            srcmask[esi / 8] |= 1 << (esi % 8);
            memcpy(psrcbuf + esi * param.T, psymbol + 2, param.T);
            srcCnt++;
        }
        else {
            repairSymbols[repairCnt++] = psymbol;
        }
        psymbol += 2 + param.T;
    }

    // second round, fill the repair symbols for the missing src symbols
    uint16_t repairIdx = 0;
    for (uint16_t i = 0; srcCnt < K && i < K; i++) {
        if ((srcmask[i / 8] & (1 << (i % 8))) == 0) {
            assert(repairIdx < repairCnt);
            uint16_t esi = *(uint16_t*)repairSymbols[repairIdx];
            // revise A for the repair symbol
            A.ZeroRow(param.S + param.H + i);
            int num = FECRQParam::GetEncodedIntermediateSymbolIdx(param,
                FECRQParam::ESIToISI(param, esi), idxes, 256);
            for (int j = 0; j < num; j++) {
                A.SetAt(param.S + param.H + i, idxes[j], 1);
            }
            // copy into repair symbol
            memcpy(psrcbuf + i * param.T, repairSymbols[repairIdx] + 2, param.T);

            repairIdx++;
            srcCnt++;
        }
    }

    // zero K' - K rows
    if (param.KP > param.K) {
        memset(psrcbuf + K * param.T, 0, (param.KP - param.K) * param.T);
    }

    // fill overhead symbols
    assert(repairCnt - repairIdx == overhead);
    int overheadIdx = 0;
    for (; repairIdx < repairCnt; repairIdx++) {
        uint16_t esi = *(uint16_t*)repairSymbols[repairIdx];
        // don't need zero the overhead rows, they are zeroed when loading A
        int num = FECRQParam::GetEncodedIntermediateSymbolIdx(param,
            FECRQParam::ESIToISI(param, esi), idxes, 256);
        for (int j = 0; j < num; j++) {
            A.SetAt(param.L + overheadIdx, idxes[j], 1);
        }

        memcpy(dbuf + (param.L + overheadIdx) * param.T, repairSymbols[repairIdx] + 2, param.T);
        overheadIdx++;
    }

    delete[] repairSymbols;

    // decoding
    FECRQVector D(dbuf, param.L + overhead, param.T, true);
    FECRQVector C = FECRQDecoding(A, D, param);
    if (C.RawData() == nullptr) {
        return nullptr; // failure
    }

    // regenerate the payload
    // for those existing src symbols, don't need recover from decoder
    psymbol = symbols;
    uint8_t *payload = new uint8_t[param.K * param.T];
    srcCnt = 0;
    for (uint16_t i = 0; i < symbolNum; i++) {
        uint16_t esi = *(uint16_t*)psymbol;
        if (esi < K) {
            memcpy(payload + esi * param.T, psymbol + 2, param.T);
            srcCnt++;
        }
        psymbol += 2 + param.T;
    }
    for (uint16_t i = 0; srcCnt < K && i < K; i++) {
        if ((srcmask[i / 8] & (1 << (i % 8))) == 0) {
            srcCnt++;
            FECRQMakeSymbol(param, i, C, payload + i * param.T);
        }
    }
    return payload;
}
