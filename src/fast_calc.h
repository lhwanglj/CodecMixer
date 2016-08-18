
#ifndef _FEC_FAST_CALC_H
#define _FEC_FAST_CALC_H

#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <emmintrin.h>
#include <smmintrin.h>

#ifndef _MM_ALIGN16
#define _MM_ALIGN16 __attribute__((aligned(16)))
#endif

// from fecrepo2_8_200.cpp
extern uint8_t _cachedOctetMul[];
extern uint8_t _cachedOctetDiv[];

namespace fec {

#define CALCU_ALIGNMENT(a, n) (((a) + (n - 1)) & (~(n - 1)))

    struct Vector { // all data aligned to 16bytes
        int rowNum;
        int rowSize;      // the real row data size <= rowStride
        int rowStride;    // the row buffer size used to align the row to 16bytes, must be times of 16bytes
        int *rowIdxes;    // length = rowNum; has the real row data idx to rowData
        uint8_t *data; // length = rowNum * rowSize
    };

    struct Matrix { // aligned matrix also
        int rowNum;
        int colNum; // colNum <= rowStride
        int rowStride;
        int *rowIdxes;
        uint8_t *data;
    };

//// common
    inline uint8_t OctetMul(uint8_t a, uint8_t b) {
        return _cachedOctetMul[((uint16_t)a << 8) | b];
    }

    inline uint8_t OctetDiv(uint8_t a, uint8_t b) {
        assert(b != 0);
        return _cachedOctetDiv[((uint16_t)b << 8) | a];
    }

    // row1 ^= row2 * multiple
    inline void XorRows(uint8_t *row1, const uint8_t *row2, uint8_t factor, int stride) {
        if (factor == 0) {
            return;
        }

        if (factor == 1) {
            int loop = stride >> 4;
            // good, it is only need xor operation on 2 rows
            while (loop-- > 0) {
                __m128i t = _mm_load_si128((const __m128i*)row1);
                __m128i s = _mm_load_si128((const __m128i*)row2);
                t = _mm_xor_si128(t, s);
                _mm_store_si128((__m128i*)row1, t);
                row1 += 16;
                row2 += 16;
            }
        }
        else {
            _MM_ALIGN16 uint8_t tmp[8];
            _MM_ALIGN16 uint16_t mulptr[8];
            __m128i f = _mm_set1_epi16((short)factor << 8);
            int loop = stride >> 3;
            while (loop-- > 0) {
                if (*(uint64_t*)row2 == 0) {
                    row1 += 8;
                    row2 += 8;
                    continue;
                }

                // get 8 mul result ptr
                __m128i t = _mm_loadl_epi64((const __m128i*)row2);
                __m128i c = _mm_cvtepu8_epi16(t);
                c = _mm_or_si128(c, f);
                _mm_store_si128((__m128i*)mulptr, c);

                // get mul result from ptr
                for (int i = 0; i < 8; i++) {
                    tmp[i] = _cachedOctetMul[mulptr[i]];
                }

                // xor the mul result to target row
                t = _mm_loadl_epi64((const __m128i*)row1);
                c = _mm_loadl_epi64((const __m128i*)tmp);
                t = _mm_xor_si128(t, c);
                _mm_storel_epi64((__m128i*)row1, t);
                row1 += 8;
                row2 += 8;
            }
        }
    }

    // row1 /= factor
    inline void DivRow(uint8_t *data, uint8_t factor, int stride) {
        assert(factor != 0);
        if (factor == 1) {
            return;
        }

        _MM_ALIGN16 uint16_t divptr[8];
        __m128i f = _mm_set1_epi16((short)factor << 8);
        int loop = stride >> 3;
        while (loop-- > 0) {
            if (*(uint64_t*)data == 0) {
                data += 8;
                continue;
            }

            __m128i t = _mm_loadl_epi64((const __m128i*)data);
            __m128i c = _mm_cvtepu8_epi16(t);
            c = _mm_or_si128(c, f);
            _mm_store_si128((__m128i*)divptr, c);

            for (int i = 0; i < 8; i++) {
                data[i] = _cachedOctetDiv[divptr[i]];
            }

            data += 8;
        }
    }

    // sum = SUM(row1 * row2)
    inline uint8_t AccumuSumAndMultipleRows(uint8_t *row1, const uint8_t *row2, int i, int stride) {
        int loop = i / 8;
        if (i % 8 != 0) {
            loop++;
        }

        while (*(uint64_t*)row1 == 0 || *(uint64_t*)row2 == 0) {
            if (--loop == 0) {
                return 0;
            }
            row1 += 8;
            row2 += 8;
        }

        // get the first 8 result and let it stay in register
        _MM_ALIGN16 uint16_t mulptr[8];
        _MM_ALIGN16 uint16_t mulret[8];

        // get first 8 mul result ptr
        __m128i t = _mm_loadl_epi64((const __m128i*)row1);
        __m128i c = _mm_cvtepu8_epi16(t);
        t = _mm_slli_epi16(c, 8);

        __m128i t2 = _mm_loadl_epi64((const __m128i*)row2);
        c = _mm_cvtepu8_epi16(t2);
        t = _mm_or_si128(t, c);

        _mm_store_si128((__m128i*)mulptr, t);
        for (int i = 0; i < 8; i++) {
            mulret[i] = _cachedOctetMul[mulptr[i]];
        }
        // now 'sum' has the first 8 multipled result
        __m128i sum = _mm_load_si128((const __m128i*)mulret);
        row1 += 8;
        row2 += 8;
        loop--;

        while (loop-- > 0) {
            if (*(uint64_t*)row1 == 0 || *(uint64_t*)row2 == 0) {
                row1 += 8;
                row2 += 8;
                continue;
            }

            t = _mm_loadl_epi64((const __m128i*)row1);
            c = _mm_cvtepu8_epi16(t);
            t = _mm_slli_epi16(c, 8);
            t2 = _mm_loadl_epi64((const __m128i*)row2);
            c = _mm_cvtepu8_epi16(t2);
            t = _mm_or_si128(t, c);

            _mm_store_si128((__m128i*)mulptr, t);
            for (int i = 0; i < 8; i++) {
                mulret[i] = _cachedOctetMul[mulptr[i]];
            }
            t = _mm_load_si128((const __m128i*)mulret);
            // add with sum
            sum = _mm_xor_si128(sum, t);

            row1 += 8;
            row2 += 8;
        }

        // add in the 8 sum to final one byte
        uint8_t finalSum = 0;
        _mm_store_si128((__m128i*)mulret, sum);
        for (int i = 0; i < 8; i++) {
            finalSum = finalSum ^ (uint8_t)(mulret[i]);
        }
        return finalSum;
    }

//// vector
    inline void Vector_SwapRows(Vector &vec, int row1, int row2) {
        int tmpIdx = vec.rowIdxes[row1];
        vec.rowIdxes[row1] = vec.rowIdxes[row2];
        vec.rowIdxes[row2] = tmpIdx;
    }

    // row1 ^= row2 * multiple
    inline void Vector_XorRows(Vector &vec, int row1, int row2, uint8_t factor) {
        uint8_t *pTargetRow = vec.data + vec.rowIdxes[row1] * vec.rowStride;
        uint8_t *pSrcRow = vec.data + vec.rowIdxes[row2] * vec.rowStride;
        XorRows(pTargetRow, pSrcRow, factor, vec.rowStride);
    }

    // row1 /= factor
    inline void Vector_DivRow(Vector &vec, int row1, uint8_t factor) {
        uint8_t *pTargetRow = vec.data + vec.rowIdxes[row1] * vec.rowStride;
        DivRow(pTargetRow, factor, vec.rowStride);
    }

    // sub_Vector = Vector.block(0, i)
    // sub_Vector = X(0, 0, i, i) * sub_Vector
    // the padding part of met must be zero
    inline void Vector_MulMatrix(Vector &vec, const Matrix &mat, int i, Vector &rotatedVec) {
        int stride = CALCU_ALIGNMENT(i, 16);
        assert(i <= mat.rowNum && i <= mat.colNum);
        assert(i <= vec.rowNum);
        assert(stride == rotatedVec.rowStride);
        assert(vec.rowSize == rotatedVec.rowNum);

        // rotation the vec
        int rowSize = vec.rowSize;
        int rowStride = vec.rowStride;
        int *rowIdxes = vec.rowIdxes;
        uint8_t *rowData = vec.data;
        for (int r = 0; r < rowSize; r++) {
            uint8_t *vec2row = rotatedVec.data + (stride * r);
            for (int j = 0; j < i; j++) {
                *vec2row++ = rowData[r + rowIdxes[j] * rowStride];
            }
            if (stride > i) {
                memset(vec2row, 0, stride - i);
            }
        }

        for (int r = 0; r < i; r++) {
            uint8_t *targetRow = rowData + rowIdxes[r] * rowStride;
            uint8_t *matrixRow = mat.data + mat.rowStride * (mat.rowIdxes == nullptr ? r : mat.rowIdxes[r]);
            uint8_t *vec2Row = rotatedVec.data;
            for (int k = 0; k < rowSize; k++) {
                *targetRow++ = AccumuSumAndMultipleRows(matrixRow, vec2Row, i, stride);
                vec2Row += stride;
            }
        }
    }

    inline void Vector_Reorder(Vector &vec, const uint16_t *c, int numOfC, int *tmpBuf) {
        for (int i = 0; i < numOfC; i++) {
            tmpBuf[c[i]] = vec.rowIdxes[i];
        }
        for (int i = numOfC; i < vec.rowNum; i++) {
            tmpBuf[i] = -1; // mark invalid for useless rows
        }
        memcpy(vec.rowIdxes, tmpBuf, sizeof(int) * vec.rowNum);
    }

//// Matrix
    inline uint8_t Matrix_GetAt(Matrix &mat, int row, int col) {
        return mat.data[mat.rowIdxes[row] * mat.rowStride + col];
    }

    inline void Matrix_SwapRows(Matrix &mat, int row1, int row2) {
        int tmpIdx = mat.rowIdxes[row1];
        mat.rowIdxes[row1] = mat.rowIdxes[row2];
        mat.rowIdxes[row2] = tmpIdx;
    }

    inline void Matrix_SwapColumns(Matrix &mat, int col1, int col2) {
        if (col1 != col2) {
            int stride = mat.rowStride;
            for (int i = 0; i < mat.rowNum; i++) {
                int rowOffset = i * stride;
                uint8_t tmp = mat.data[rowOffset + col1];
                mat.data[rowOffset + col1] = mat.data[rowOffset + col2];
                mat.data[rowOffset + col2] = tmp;
            }
        }
    }

    // row1 ^= row2 * multiple
    inline void Matrix_XorRows(Matrix &mat, int row1, int row2, uint8_t factor) {
        uint8_t *pTargetRow = mat.data + mat.rowIdxes[row1] * mat.rowStride;
        uint8_t *pSrcRow = mat.data + mat.rowIdxes[row2] * mat.rowStride;
        XorRows(pTargetRow, pSrcRow, factor, mat.rowStride);
    }

    // row1 /= factor
    inline void Matrix_DivRow(Matrix &mat, int row1, uint8_t factor) {
        uint8_t *pTargetRow = mat.data + mat.rowIdxes[row1] * mat.rowStride;
        DivRow(pTargetRow, factor, mat.rowStride);
    }

    // sub_X = X.block(0, 0, i, i);
    // sub_A = A.block(0, 0, i, A.cols());
    // sub_A = sub_X * sub_A;
    inline void Matrix_MulMatrix(Matrix &targetMat, Matrix &srcMat, int i, Matrix &rotatedMat) {
        int stride = CALCU_ALIGNMENT(i, 16);
        assert(i <= targetMat.rowNum);
        assert(stride == rotatedMat.rowStride);
        assert(targetMat.colNum == rotatedMat.rowNum);

        // rotation the mat
        int rows = targetMat.colNum;
        int rowStride = targetMat.rowStride;
        int *rowIdxes = targetMat.rowIdxes;
        uint8_t *matdata = targetMat.data;
        for (int r = 0; r < rows; r++) {
            uint8_t *mat2row = rotatedMat.data + (stride * r);
            for (int j = 0; j < i; j++) {
                *mat2row++ = matdata[r + rowIdxes[j] * rowStride];
            }
            if (stride > i) {
                memset(mat2row, 0, stride - i);
            }
        }

        for (int r = 0; r < i; r++) {
            uint8_t *targetRow = matdata + rowIdxes[r] * rowStride;
            uint8_t *matrixRow = srcMat.data + srcMat.rowStride * srcMat.rowIdxes[r];
            uint8_t *mat2Row = rotatedMat.data;
            for (int k = 0; k < rows; k++) {
                *targetRow++ = AccumuSumAndMultipleRows(matrixRow, mat2Row, i, stride);
                mat2Row += stride;
            }
        }
    }

    inline uint32_t Matrix_RowDegree(Matrix &mat, int row, int colBegin, int colEnd) {
        assert(row < mat.rowNum);
        assert(colBegin <= colEnd && colEnd <= mat.colNum);

        int rowIdx = mat.rowIdxes[row] * mat.rowStride;
        uint32_t degree = 0;
        while (colBegin < colEnd) {
            degree += mat.data[rowIdx + colBegin];
            colBegin++;
        }
        return degree;
    }

    // returning non-zero count
    inline int Matrix_CountRow(Matrix &mat, int row, int colBegin, int colEnd, 
        int &idxOfOne1, int &idxOfOne2, int maxOfR) {
        assert(row < mat.rowNum);
        assert(colBegin <= colEnd && colEnd <= mat.colNum);

        int oneCnt = 0;
        int nonZeros = 0;
        int rowIdx = mat.rowIdxes[row] * mat.rowStride;
        while (colBegin < colEnd) {
            uint8_t val = mat.data[rowIdx + colBegin];
            if (val != 0) {
                if (++nonZeros > maxOfR) {
                    break;
                }
                if (val == 1) {
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
}

#endif
