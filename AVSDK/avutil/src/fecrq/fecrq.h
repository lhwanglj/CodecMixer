
#ifndef _MEDIACLOUD_FECRQ_H
#define _MEDIACLOUD_FECRQ_H

#include <stdint.h>
#include <vector>
#include "fecrqm.h"

/// raptorQ rfc6330 implementation
class FECRQEncoder
{
public:
    
    FECRQEncoder(const uint8_t data[], int length, int symbolSize);
    // caller quarantee the last src symbol has zero padding !!
	FECRQEncoder(const uint8_t* symbols[], int payloadLength, int symbolSize);
    virtual ~FECRQEncoder();

    int NumOfSourceSymbols() const { return _param.K; }
    int SymbolSize() const { return _param.T; }

    // length >= symbolSize
    // esi < K is source symbols; otherwise it is repair symbols
    // if K * symbolSize > length, the last source symbol will be padded with zero.
    void GetEncodedSymbols(int esi, uint8_t data[]);

    // used to create the intermediate symbols explicitly
    void ForceEncoding();

private:
    FECRQEncoder(const FECRQParam::Param &param, FECRQVector &cSymbols, int length);
    void GenerateCSymbols();

    uint8_t *_dbuf;
    int _length;    // the real length;
    FECRQParam::Param _param;
    FECRQVector _cSymbols;
    FECRQMatrix _coreMatrix;

    friend class FECRQDecoder;
};

class FECRQDecoder
{
public:

    // overload determine when to start real decoding process
    FECRQDecoder(int dataLength, int symbolSize, int numOfSourceSymbols, int overload);
    virtual ~FECRQDecoder();

    int SymbolSize() const { return _param.T; }
    int NumOfSourceSymbols() const { return _param.K; }
    int DataLength() const { return _dataLength; }

    // for server decoding
    // symbols is the pointer to symbol data from esi is [0 - esiEnd)
    // no symbol have nullprt in symbols[], symbolNum is the pointer number in symbols[]
    static FECRQEncoder* Decode(int dataLength, int symbolSize, 
        const uint8_t* symbols[], uint16_t symbolNum, uint16_t esiEnd);

    // this is another way to decode for client
    // symbols is a buffer that has format of symbolNum pairs of | 2byte esi | symbol data |
    // the last src symbol has zero padding !!
    static uint8_t* Decode(int dataLength, int symbolSize,
        const uint8_t* symbols, uint16_t symbolNum);
	
    // the data length >= symbolSize
    // returning true if the decoding successfully completed
    bool AddEncodedSymbols(int esi, const uint8_t data[]);

    // esi < numOfSourceSymbols
    // returning buffer pointer directly
    const uint8_t* GetSourceSymbols(int esi) const;

    const uint8_t* GetBuffer() const { return _data; }

private:
    void TryDecoding();

    uint8_t *_data; // only save the K source symbols
    FECRQParam::Param _param;
    int _overload;
    uint8_t *_sourceSymbolMask;
    int _sourceSymbolNum;
    int _dataLength;
    std::vector<std::pair<int, uint8_t*>> _repairSymbols;
};



#endif
