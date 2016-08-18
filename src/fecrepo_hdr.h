#ifndef _FECREPO_HEADER_H
#define _FECREPO_HEADER_H

#include <stdint.h>

namespace FecRepo {

    struct MatrixInfo {
        const uint8_t *data;
        int rows;
        int cols;
        int stride;
        int L;
    };

    struct EncodeCInfo {
        const uint16_t *data;
        int L;
    };

    enum VectorOps {
        VectorOps_SwapRows = 1,
        VectorOps_XorRows,
        VectorOps_DivRow,
        VectorOps_MulMatrix,
        VectorOps_Reorder
    };

    struct EncodeVecOp {
        uint8_t ops;
        uint8_t factor;
        uint16_t row1;  // has i for MulMatrix
        uint16_t row2;
    };

    struct EncodeVecOpInfo {
        const EncodeVecOp *ops;
        int opsNum;
        int L;
    };

    struct Tuple {
        uint32_t d, a, b, d1, a1, b1;
    };

    struct TupleIndexes {
        int idxOffset; // the offset in tupleIdxData;
        uint8_t numOfIdxes; // the number of uint8_t in tupleIdxData;
                            // actually, when K <= 200, the max numOfIdxes is 32
    };

    struct TupleInfo {
        const Tuple   *tuples;    // the tuples indexed by isi
        const TupleIndexes *tupleIdxes; // it is the intermediate symbols indexes output from tuple
        const uint8_t *tupleIdxData;
        int tupleNum;
        int KP;
        int L;
    };

    struct Param {
        int K, T, KP, S, H, W, L, P, P1, U, B, J;
    };

    struct ParamInfo {
        Param param;
        int idxOfMatrixA;
        int idxOfMatrixX;
        int idxOfEncodeC;
        int idxOfEncodeOps;
        int idxOfTuples;
    };

    class RepoReader {
    public:
        const static int MinK = 8;
        const static int MaxK = 200;

        static const Param* GetParam(int K);
        /*
        * ltIdxes - a pointer to uint8_t array, each element is an index of intermediate symbols
        * numOfIdx - the size of ltIdxes array
        */
        static const Tuple* GetTuple(const Param *param, int isi, const uint8_t* &ltIdxes, uint8_t &numOfIdx);
        static const MatrixInfo*      GetMatrixA(const Param *param);
        static const MatrixInfo*      GetMatrixX(const Param *param);
        static const EncodeCInfo*     GetEncodeC(const Param *param);
        static const EncodeVecOpInfo* GetEncodeVecOps(const Param *param);
    };
}

#endif
