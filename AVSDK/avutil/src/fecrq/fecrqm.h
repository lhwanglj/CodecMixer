
#ifndef _MEDIACLOUD_FECRQM_H
#define _MEDIACLOUD_FECRQM_H

#include <stdint.h>
#include <assert.h>
#include <string>
#include <string.h>

#include <../../include/config.h>

// only two types of matrix are used in raptorq
// a octect matrix A and X
// column vector of symbol D and C

class FECRQParam
{
public:
    struct Tuple
    {
        uint32_t d, a, b, d1, a1, b1;
    };

    struct Param
    {
        int K, T, KP, S, H, W, L, P, P1, U, B, J;
    };

    static int ESIToISI(const Param &param, int esi)
    {
        if (esi < param.K)
            return esi;
        return param.KP + (esi - param.K);
    }

    static int ISIToESI(const Param &param, int isi)
    {
        assert(isi < param.K && isi >= param.KP);
        if (isi < param.K)
            return isi;
        return isi - (param.KP - param.K);
    }

    static Param GetParam(int K, int T);
    static Tuple GenTuple(const Param &param, int isi);

    // returning the idx of C for encoding the ISIth repair symbols
    // returning the number of idx
    static int GetEncodedIntermediateSymbolIdx(const Param &param, int isi, uint16_t idxes[], int idxLength);

    static uint32_t Rand(uint32_t y, uint32_t i, uint32_t m);

    // determine the K
    // if the K * symbolSize is larger than datelen, then the data need to be padded by zeros.
    // we need to minimize the delta K * symbolSize - datalen for saving the bandwidth !!
    static int DetermineNumOfSourceSymbols(int datalen, int symbolSize);
};


extern uint8_t fecOCT_EXP[];
extern uint8_t fecOCT_LOG[];
#define FECOctetAdd(a, b)   ((a) ^ (b))
#define FECOctetSub(a, b)   ((a) ^ (b))
#define FECOctetMul(a, b)   ((a) == 0 || (b) == 0 ? 0 : fecOCT_EXP[fecOCT_LOG[(a) - 1] + fecOCT_LOG[(b) - 1]])
#define FECOctetDiv(a, b)   ((a) == 0 ? 0 : fecOCT_EXP[fecOCT_LOG[(a) - 1] - fecOCT_LOG[(b) - 1] + 255])


// this is a matrix functionality wrapper on octet buffer
class FECRQMatrix
{
    uint8_t *_data;
    int _rows, _columns;
    bool _owning;

public:
    FECRQMatrix()
    {
        _data = nullptr;
        _rows = _columns = 0;
        _owning = false;
    }

    FECRQMatrix(uint8_t *data, int rows, int columns, bool ownData)
    {
        assert(data != nullptr);
        assert(rows > 0 && columns > 0);
        _data = data;
        _rows = rows;
        _columns = columns;
        _owning = ownData;
    }

    FECRQMatrix(FECRQMatrix &other)
    {
        if (other._data == nullptr)
        {
            _data = nullptr;
            _rows = _columns = 0;
            _owning = false;
        }
        else
        {
            _data = other._data;
            _rows = other._rows;
            _columns = other._columns;
            _owning = other._owning;

            if (_owning)
            {
                other._data = nullptr;
                other._rows = other._columns = 0;
                other._owning = false;
            }
        }
    }

    ~FECRQMatrix()
    {
        if (_data != nullptr && _owning)
            delete[] _data;
    }

    void SetData(uint8_t *data, int rows, int columns, bool ownData)
    {
        if (_data != nullptr && _owning)
            delete[] _data;

        _data = data;
        _rows = rows;
        _columns = columns;
        _owning = ownData;
    }

    int Rows() const { return _rows; }
    int Columns() const { return _columns; }
    uint8_t* RawData() const { return _data; }
    void Dump() const;

    FECRQMatrix& operator=(FECRQMatrix &other)
    {
        if (this == &other)
            return *this;

        if (_data != nullptr && _owning)
            delete[] _data;

        _data = other._data;
        _rows = other._rows;
        _columns = other._columns;
        _owning = other._owning;

        if (_owning)
        {
            other._data = nullptr;
            other._rows = other._columns = 0;
            other._owning = false;
        }
        return *this;
    }

    void CloneFrom(FECRQMatrix &other)
    {
        if (_data != nullptr && _owning)
            delete[] _data;

        _owning = true;
        _data = nullptr;
        _rows = other._rows;
        _columns = other._columns;
        if (other._data != nullptr)
        {
            _data = new uint8_t[_rows * _columns];
            memcpy(_data, other._data, _rows * _columns);
        }
    }
    
    // cloned matrix always own the data
    FECRQMatrix Clone() const
    {
        FECRQMatrix cloned;
        if (_data != nullptr)
        {
            uint8_t *newdata = new uint8_t[_rows * _columns];
            memcpy(newdata, _data, _rows * _columns);
            cloned.SetData(newdata, _rows, _columns, true);
        }
        return cloned;
    }

    void SetAt(int row, int col, uint8_t data)
    {
        assert(row < _rows && col < _columns);
        _data[row * _columns + col] = data;
    }

    uint8_t GetAt(int row, int col)
    {
        assert(row < _rows && col < _columns);
        return _data[row * _columns + col];
    }

    void ZeroRow(int row)
    {
        assert(row < _rows);
        memset(_data + row * _columns, 0, _columns);
    }

    void SwapRow(int row1, int row2)
    {
        assert(row1 < _rows && row2 < _rows);
        if (row1 != row2)
        {
            for (int i = 0; i < _columns; i++)
            {
                uint8_t tmp = _data[row1 * _columns + i];
                _data[row1 * _columns + i] = _data[row2 * _columns + i];
                _data[row2 * _columns + i] = tmp;
            }
        }
    }

    void SwapColumn(int col1, int col2)
    {
        assert(col1 < _columns && col2 < _columns);
        if (col1 != col2)
        {
            for (int i = 0; i < _rows; i++)
            {
                uint8_t tmp = _data[i * _columns + col1];
                _data[i * _columns + col1] = _data[i * _columns + col2];
                _data[i * _columns + col2] = tmp;
            }
        }
    }

    uint32_t RowDegree(int row, int colBegin, int colEnd)
    {
        assert(row < _rows);
        assert(colBegin <= colEnd && colEnd <= _columns);
        
        int rowIdx = row * _columns;
        uint32_t degree = 0;
        while (colBegin < colEnd)
        {
            degree += _data[rowIdx + colBegin];
            colBegin++;
        }
        return degree;
    }

    // returning non-zero count
    int CountRow(int row, int colBegin, int colEnd, int &idxOfOne1, int &idxOfOne2, int maxOfR)
    {
        assert(row < _rows);
        assert(colBegin <= colEnd && colEnd <= _columns);

        int nonZeros = 0;
        int rowIdx = row * _columns;
        int oneCnt = 0;
        while (colBegin < colEnd)
        {
            uint8_t val = _data[rowIdx + colBegin];
            if (val != 0)
            {
                if (++nonZeros > maxOfR)
                    break;
                if (val == 1)
                {
                    if (oneCnt == 0)
                        idxOfOne1 = colBegin;
                    else if (oneCnt == 1)
                        idxOfOne2 = colBegin;
                    oneCnt++;
                }
            }
            colBegin++;
        }
        return nonZeros;
    }

    // targetRow += sourceRow * multiple
    void AddInPlace(int targetRow, int sourceRow, uint8_t multiple)
    {
        assert(targetRow < _rows && sourceRow < _rows);
        int targetRowIdx = targetRow * _columns;
        int sourceRowIdx = sourceRow * _columns;
        for (int i = 0; i < _columns; i++)
            _data[targetRowIdx + i] = FECOctetAdd(_data[targetRowIdx + i], FECOctetMul(_data[sourceRowIdx + i], multiple));
    }

    // targetRow -= sourceRow * multiple
    void SubInPlace(int targetRow, int sourceRow, uint8_t multiple)
    {
        assert(targetRow < _rows && sourceRow < _rows);
        int targetRowIdx = targetRow * _columns;
        int sourceRowIdx = sourceRow * _columns;
        for (int i = 0; i < _columns; i++)
            _data[targetRowIdx + i] = FECOctetSub(_data[targetRowIdx + i], FECOctetMul(_data[sourceRowIdx + i], multiple));
    }

    void DivRow(int row, uint8_t divisor)
    {
        assert(row < _rows);
        assert(divisor != 0);

        int rowIdx = row * _columns;
        for (int i = 0; i < _columns; i++)
            _data[rowIdx + i] = FECOctetDiv(_data[rowIdx + i], divisor);
    }

    // sub_X = X.block(0, 0, i, i);
    // sub_A = A.block(0, 0, i, A.cols());
    // sub_A = sub_X * sub_A;
    void MulMatrix(FECRQMatrix &X, int i)
    {
        assert(i > 0 && i <= _rows);
        assert(i <= X.Rows());

        uint8_t *tmp = new uint8_t[i * _columns];
        uint8_t *xdata = X.RawData();
        int xCols = X.Columns();

        for (int j = 0; j < i; j++)
            for (int k = 0; k < _columns; k++)
            {
                uint8_t sum = 0;
                int xRowIdx = j * xCols;
                for (int w = 0; w < i; w++)
                    sum = FECOctetAdd(sum, FECOctetMul(xdata[xRowIdx + w], _data[w * _columns + k]));
                tmp[j * _columns + k] = sum;
            }

        memcpy(_data, tmp, i * _columns);
        delete[] tmp;
    }

    // A[HDPC] = MT * GAMMA
    void MulMatrix2(FECRQMatrix &MT, FECRQMatrix &GAMMA, int startRow, int rows, int cols)
    {
        uint8_t *mtdata = MT.RawData();
        uint8_t *gdata = GAMMA.RawData();
        for (int j = 0; j < rows; j++)
        {
            for (int k = 0; k < cols; k++)
            {
                uint8_t sum = 0;
                int mtRowIdx = j * cols;
                for (int w = 0; w < cols; w++)
                    sum = FECOctetAdd(sum, FECOctetMul(mtdata[mtRowIdx + w], gdata[w * cols + k]));
                _data[(startRow + j) * _columns + k] = sum;
            }
        }
    }
};


class FECRQVector
{
    uint8_t *_data;
    int _rows;
    int _eleSize;
    bool _owning;

public:
    FECRQVector()
    {
        _data = nullptr;
        _rows = _eleSize = 0;
        _owning = false;
    }

    FECRQVector(uint8_t *data, int rows, int vecSize, bool ownData)
    {
        assert(data != nullptr);
        assert(rows > 0 && vecSize > 0);
        _data = data;
        _rows = rows;
        _eleSize = vecSize;
        _owning = ownData;
    }

    FECRQVector(const FECRQVector &other)
    {
        if (other._data == nullptr)
        {
            _data = nullptr;
            _rows = _eleSize = 0;
            _owning = false;
        }
        else
        {
            _data = other._data;
            _rows = other._rows;
            _eleSize = other._eleSize;
            _owning = other._owning;

            if (_owning)
            {
                FECRQVector *pother = (FECRQVector*)&other;
                pother->_data = nullptr;
                pother->_rows = pother->_eleSize = 0;
                pother->_owning = false;
            }
        }
    }

    ~FECRQVector()
    {
        if (_data != nullptr && _owning)
            delete[] _data;
    }

    void SetData(uint8_t *data, int rows, int vecSize, bool ownData)
    {
        assert(data != nullptr);
        assert(rows > 0 && vecSize > 0);

        if (_data != nullptr && _owning)
            delete[] _data;

        _data = data;
        _rows = rows;
        _eleSize = vecSize;
        _owning = ownData;
    }

    int Rows() const { return _rows; }
    int VecSize() const { return _eleSize; }
    uint8_t* RawData() const { return _data; }
    void Dump() const;
    void Dump(int d[]) const;

    FECRQVector& operator=(const FECRQVector &other)
    {
        if (this == &other)
            return *this;

        if (_data != nullptr && _owning)
            delete[] _data;

        _data = other._data;
        _rows = other._rows;
        _eleSize = other._eleSize;
        _owning = other._owning;

        if (_owning)
        {
            FECRQVector *pother = (FECRQVector*)&other;
            pother->_data = nullptr;
            pother->_rows = pother->_eleSize = 0;
            pother->_owning = false;
        }
        return *this;
    }

    FECRQVector Clone() const
    {
        FECRQVector cloned;
        if (_data != nullptr)
        {
            uint8_t *newdata = new uint8_t[_rows * _eleSize];
            memcpy(newdata, _data, _rows * _eleSize);
            cloned.SetData(newdata, _rows, _eleSize, true);
        }
        return cloned;
    }

    void CloneFrom(FECRQVector &other) 
    {
        if (_data != nullptr && _owning)
            delete[] _data;

        _owning = true;
        _data = nullptr;
        _rows = other._rows;
        _eleSize = other._eleSize;
        if (other._data != nullptr)
        {
            _data = new uint8_t[_rows * _eleSize];
            memcpy(_data, other._data, _rows * _eleSize);
        }
    }

    uint8_t* GetRow(int row) const
    {
        assert(row < _rows);
        return _data + row * _eleSize;
    }

    void CopyRow(int targetRow, FECRQVector &other, int sourceRow)
    {
        assert(targetRow < _rows);
        assert(sourceRow < other._rows);
        assert(_eleSize == other._eleSize);
        memcpy(_data + targetRow * _eleSize, other._data + sourceRow * _eleSize, _eleSize);
    }

    // targetRow += sourceRow * multiple
    void AddInPlace(int targetRow, int sourceRow, uint8_t multiple)
    {
        assert(targetRow < _rows);
        assert(sourceRow < _rows);
        int targetIdx = targetRow * _eleSize;
        int sourceIdx = sourceRow * _eleSize;
        for (int i = 0; i < _eleSize; i++)
            _data[targetIdx + i] = FECOctetAdd(_data[targetIdx + i], FECOctetMul(_data[sourceIdx + i], multiple));
    }

    // targetRow -= sourceRow * multiple
    void SubInPlace(int targetRow, int sourceRow, uint8_t multiple)
    {
        assert(targetRow < _rows);
        assert(sourceRow < _rows);
        int targetIdx = targetRow * _eleSize;
        int sourceIdx = sourceRow * _eleSize;
        for (int i = 0; i < _eleSize; i++)
            _data[targetIdx + i] = FECOctetSub(_data[targetIdx + i], FECOctetMul(_data[sourceIdx + i], multiple));
    }

    void DivRow(int row, uint8_t divisor)
    {
        assert(row < _rows);
        assert(divisor != 0);

        int rowIdx = row * _eleSize;
        for (int i = 0; i < _eleSize; i++)
            _data[rowIdx + i] = FECOctetDiv(_data[rowIdx + i], divisor);
    }

    void AddRow(int row, FECRQVector &other, int otherRow)
    {
        assert(row < _rows);
        assert(otherRow < other._rows);
        assert(_eleSize == other._eleSize);

        int targetIdx = row * _eleSize;
        int sourceIdx = otherRow * _eleSize;
        for (int i = 0; i < _eleSize; i++)
            _data[targetIdx + i] = FECOctetAdd(_data[targetIdx + i], other._data[sourceIdx + i]);
    }

    // sub_Vector = Vector.block(0, i)
    // sub_Vector = X(0, 0, i, i) * sub_Vector
    // the real row data is instructed by d
    void MulMatrix(FECRQMatrix &X, int i, FECRQVector &D2, int d[])
    {
        assert(i > 0 && i <= _rows && i <= X.Rows());
        assert(_eleSize == D2._eleSize);

        int xCols = X.Columns();
        uint8_t *xdata = X.RawData();
        uint8_t *d2Data = D2.RawData();

        for (int j = 0; j < i; j++)
            for (int k = 0; k < _eleSize; k++)
            {
                uint8_t sum = 0;
                int xRowIdx = j * xCols;
                for (int w = 0; w < i; w++)
                    sum = FECOctetAdd(sum, FECOctetMul(xdata[xRowIdx + w], d2Data[d[w] * _eleSize + k]));
                _data[d[j] * _eleSize + k] = sum;
            }
    }
};

#endif
