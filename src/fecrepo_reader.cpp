
#include <assert.h>
#include "fecrepo_hdr.h"

using namespace FecRepo;

extern MatrixInfo _matrix_A_repos[];
extern MatrixInfo _matrix_X_repos[];
extern EncodeCInfo _encode_C_repos[];
extern EncodeVecOpInfo _encode_Vector_Ops_repos[];
extern TupleInfo _tuples_repo[];
extern ParamInfo _param_repo[];

const Param* RepoReader::GetParam(int K) 
{
    assert(K >= MinK && K <= MaxK);
    return &_param_repo[K - MinK].param;
}

const Tuple* RepoReader::GetTuple(const Param *param, int isi, 
    const uint8_t* &ltIdxes, uint8_t &numOfIdx)
{
    assert(param->K >= MinK && param->K <= MaxK);
    const TupleInfo &tupleInfo = _tuples_repo[_param_repo[param->K - MinK].idxOfTuples];
    if (isi < tupleInfo.tupleNum) {
        ltIdxes = tupleInfo.tupleIdxData + tupleInfo.tupleIdxes[isi].idxOffset;
        numOfIdx = tupleInfo.tupleIdxes[isi].numOfIdxes;
        return tupleInfo.tuples + isi;
    }
    ltIdxes = nullptr;
    numOfIdx = 0;
    return nullptr;
}

const MatrixInfo* RepoReader::GetMatrixA(const Param *param)
{
    assert(param->K >= MinK && param->K <= MaxK);
    return &_matrix_A_repos[_param_repo[param->K - MinK].idxOfMatrixA];
}

const MatrixInfo* RepoReader::GetMatrixX(const Param *param)
{
    assert(param->K >= MinK && param->K <= MaxK);
    return &_matrix_X_repos[_param_repo[param->K - MinK].idxOfMatrixX];
}

const EncodeCInfo* RepoReader::GetEncodeC(const Param *param)
{
    assert(param->K >= MinK && param->K <= MaxK);
    return &_encode_C_repos[_param_repo[param->K - MinK].idxOfEncodeC];
}

const EncodeVecOpInfo* RepoReader::GetEncodeVecOps(const Param *param)
{
    assert(param->K >= MinK && param->K <= MaxK);
    return &_encode_Vector_Ops_repos[_param_repo[param->K - MinK].idxOfEncodeOps];
}
