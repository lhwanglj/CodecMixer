
#include <assert.h>
#include <vector>
#include <tuple>
#include "fast_fec.h"
#include "fast_calc.h"

using namespace fec;

void DumpMatrix(Matrix &A) {
    printf("dumping matrix [%d, %d, %d] --->\n", A.rowNum, A.colNum, A.rowStride);
    for (int i = 0; i < A.rowNum; i++) {
        for (int j = 0; j < A.rowStride; j++) {
            printf("%2x ", A.data[A.rowIdxes[i] * A.rowStride + j]);
        }   
        printf("\n");
    }
}

void DumpVector(Vector &vec) {
    printf("dumping vector [%d, %d, %d] --->\n", vec.rowNum, vec.rowSize, vec.rowStride);
    for (int i = 0; i < vec.rowNum; i++) {
        for (int j = 0; j < vec.rowStride; j++) {
            printf("%2x ", vec.data[vec.rowIdxes[i] * vec.rowStride + j]);
        }
        printf("\n");
    }
}

class Graph {
    static const int MaxConnections = 250;
    struct Pair { 
        short first; 
        short second; 
    } _connections[MaxConnections];

    int _maxConnections;
    int _numOfConnections;
    int _maxSize;
    int _curSize;

    // find the root node of a sub component in this graph
    inline int Find(int id) const {
        int tmp = id;
        while (_connections[tmp].second != tmp)
            tmp = _connections[tmp].second;
        return tmp;
    }

public:
    Graph(int maxSize) {
        assert(maxSize <= MaxConnections);
        _maxSize = maxSize;
        _curSize = 0;
        _maxConnections = 0;
        _numOfConnections = 0;
    }

    inline void Reset(int size) {
        assert(size > 0 && size <= _maxSize);
        _curSize = size;
        _maxConnections = 0;
        _numOfConnections = 0;
        for (int i = 0; i < size; i++) {
            _connections[i].first = 1;
            _connections[i].second = i;
        }
    }

    inline int NumOfConnection() const { return _numOfConnections; }

    inline void Connect(int node_a, int node_b) {
        // initialize if the connection number is zero
        assert(node_a < node_b);
        assert(node_b < _curSize);

        _numOfConnections++;

        int rep_a = Find(node_a);
        int rep_b = Find(node_b);
        if (rep_a == rep_b) {
            // they already in the same component
            return;
        }

        _connections[rep_a] = { short(_connections[rep_a].first + _connections[rep_b].first), short(rep_a) };
        _connections[rep_b] = _connections[rep_a];
        if (node_a != rep_a)
            _connections[node_a] = _connections[rep_a];
        if (node_b != rep_b)
            _connections[node_b] = _connections[rep_b];

        if (_maxConnections < _connections[rep_a].first)
            _maxConnections = _connections[rep_a].first;
    }

    inline bool IsMax(int id) const {
        assert(id < _curSize);
        return _maxConnections == _connections[Find(id)].first;
    }
};

// A and X is [M * L], D is [L * 1]
static std::tuple<bool, int, int> DecodingFirstly(Matrix &A, Matrix &X, Vector &D,
    uint16_t c[], const Param &param, IAllocator *allocator)
{
    struct IntPair { int first; int second; };
    static_assert(sizeof(IntPair) == 8, "IntPair is too large");

    int i = 0;
    int u = param.P;  // come from p
    int S = param.S;
    int H = param.H;
    int L = param.L;
    int M = A.rowNum;

    // track hdpc rows and original degree of each row
    IntPair *tracking = (IntPair*)allocator->AllocFecBuffer(M * sizeof(IntPair), 0, IAllocator::PurposeUnknown);
    for (int j = 0; j < M; j++) {
        uint32_t degree = Matrix_RowDegree(A, j, 0, L - u);
        bool isHDPC = j >= S && j < S + H;
        tracking[j] = { isHDPC, int(degree) };
    }

    int loopCnt = 0;

    // pair is row idx, the first idx of one column
    int rRowsCount = 0;
    IntPair *rRows = (IntPair*)allocator->AllocFecBuffer(M * sizeof(IntPair), 0, IAllocator::PurposeUnknown);

    // for determine the max size component in V graph
    Graph graph(L - u);

    while (i + u < L) {
        rRowsCount = 0;
        graph.Reset(L - u - i);

        // find the row
        int r = M;
        bool twoOnes = false;
        for (int j = i; j < M; j++) {
            int idxOfOne1 = -1, idxOfOne2 = -1;
            int nonZeros = Matrix_CountRow(A, j, i, L - u, idxOfOne1, idxOfOne2, r);
            if (nonZeros > r || nonZeros == 0) {
                continue;
            }

            if (nonZeros < r) {
                r = nonZeros;
                rRowsCount = 0;
            }

            // noneZeros == r
            bool inGraph = false;
            if (r == 2) {
                if (idxOfOne1 >= 0 && idxOfOne2 >= 0) {
                    // 2 ones row
                    twoOnes = true;
                    if (0 == tracking[j].first) {
                        // only the row which is in graph has valid second value of pair
                        graph.Connect(idxOfOne1 - i, idxOfOne2 - i);
                        rRows[rRowsCount++] = { j, idxOfOne1 };
                        inGraph = true;
                    }
                }
            }

            if (!inGraph) {
                rRows[rRowsCount++] = { j, L };
            }
        }

        // If all entries of V are zero, then no row is chosen and decoding fails.
        if (r == M) {
            allocator->FreeFecBuffer(tracking, IAllocator::PurposeUnknown);
            allocator->FreeFecBuffer(rRows, IAllocator::PurposeUnknown);
            return std::make_tuple(false, 0, 0);	// failure
        }

        // choose the row
        int chosen = M;
        if (r != 2) {
            // If r != 2, then choose a row with exactly r nonzeros in V with
            // minimum original degree among all such rows, except that HDPC
            // rows should not be chosen until all non - HDPC rows have been
            int minRowIdx = M;
            uint32_t minRowDegree = UINT32_MAX;
            int minHDPCRowIdx = M;
            uint32_t minHDPCRowDegree = UINT32_MAX;

            for (int p = 0; p < rRowsCount; p++) {
                IntPair &pair = rRows[p];
                uint32_t degree = tracking[pair.first].second;
                if (tracking[pair.first].first != 0) {
                    if (degree < minHDPCRowDegree) {
                        minHDPCRowDegree = degree;
                        minHDPCRowIdx = pair.first;
                    }
                }
                else {
                    if (degree < minRowDegree) {
                        minRowDegree = degree;
                        minRowIdx = pair.first;
                    }
                }
            }

            chosen = minRowIdx < M ? minRowIdx : minHDPCRowIdx;
        }
        else {
            // If r = 2 and there is a row with exactly 2 ones in V, then
            // choose any row with exactly 2 ones in V that is part of a
            // maximum size component in the graph described above that is
            // defined by V.
            // If r = 2 and there is no row with exactly 2 ones in V, then
            // choose any row with exactly 2 nonzeros in V.
            if (graph.NumOfConnection() > 0) {
                for (int p = 0; p < rRowsCount; p++) {
                    IntPair &pair = rRows[p];
                    if (pair.second < L) {
                        // this row is in graph
                        if (graph.IsMax(pair.second - i)) {
                            chosen = pair.first;
                            break;
                        }
                    }
                }
            }

            if (chosen == M) {
                chosen = rRows[0].first;
            }
        }

        assert(chosen >= i && chosen < M);

        // swap chosen row and first V row in A (not just in V)
        if (chosen != i) {
            Matrix_SwapRows(A, i, chosen);
            Matrix_SwapRows(X, i, chosen);
            Vector_SwapRows(D, i, chosen);
            std::swap(tracking[i], tracking[chosen]);
        }

        //The columns of A among those
        //that intersect V are reordered so that one of the r nonzeros in the
        //chosen row appears in the first column of V and so that the remaining
        //r - 1 nonzeros appear in the last columns of V.
        if (Matrix_GetAt(A, i, i) == 0) {
            for (int j = i + 1; j < L - u; j++) {
                if (Matrix_GetAt(A, i, j) != 0) {
                    Matrix_SwapColumns(A, i, j);
                    Matrix_SwapColumns(X, i, j);
                    std::swap(c[i], c[j]);
                    break;
                }
            }
        }

        int cntOfRestNonZero = r - 1; // there are this number of non-zero from i + 1 to L - u
        int cntOfTailNonZero = 0;
        for (int j = L - u - 1; j > i && cntOfRestNonZero > 0; j--) {
            if (Matrix_GetAt(A, i, j) != 0) {
                int swapIdx = L - u - 1 - cntOfTailNonZero;
                if (j != swapIdx) {
                    Matrix_SwapColumns(A, j, swapIdx);
                    Matrix_SwapColumns(X, j, swapIdx);
                    std::swap(c[j], c[swapIdx]);
                }
                cntOfRestNonZero--;
                cntOfTailNonZero++;
            }
        }

        //Then, an
        //appropriate multiple of the chosen row is added to all the other rows
        //of A below the chosen row that have a nonzero entry in the first
        //column of V.Specifically, if a row below the chosen row has entry
        //beta in the first column of V, and the chosen row has entry alpha in
        //the first column of V, then beta / alpha multiplied by the chosen row
        //is added to this row to leave a zero value in the first column of V.
        uint8_t alpha = Matrix_GetAt(A, i, i);
        for (int j = i + 1; j < M; j++) {
            uint8_t beta = Matrix_GetAt(A, j, i);
            if (beta == 0)
                continue;

            uint8_t divisor = OctetDiv(beta, alpha);
            Matrix_XorRows(A, j, i, divisor);
            Vector_XorRows(D, j, i, divisor);
        }

//        printf("decoding phase1, loop %d, i is %d, u is %d, r is %d, twoones %d, chose %d\n", loopCnt, i, u, r, twoOnes, chosen);
//        DumpMatrix(A);

        i++;
        u += r - 1;
        loopCnt++;
    }

    allocator->FreeFecBuffer(tracking, IAllocator::PurposeUnknown);
    allocator->FreeFecBuffer(rRows, IAllocator::PurposeUnknown);

    assert(i + u == L);
    return std::make_tuple(true, i, u);
}

// make a Gaussian elimination on A's U_lower sub matrix
static bool DecodingSecondly(Matrix &A, Vector &D, int i, int u, const Param &param)
{
    //printf("DecodingSecondly, i = %d, u = %d\n", i, u);
    //DumpMatrix(A);
    //DumpVector(D);

    int fromRow = i, toRow = A.rowNum;
    int toCol = param.L;
    int lead = param.L - u;
    for (int r = fromRow; r < toRow && lead < toCol; r++) {
        int chosen = r;
        while (Matrix_GetAt(A, chosen, lead) == 0) {
            if (++chosen == toRow)
                break;
        }

        if (chosen == toRow) {
            // all zeros in this col, error
            // the final U_lower will not be u rank
            return false;
        }

        if (chosen != r) {
            Matrix_SwapRows(A, r, chosen);
            Vector_SwapRows(D, r, chosen);
        }

        uint8_t leadval = Matrix_GetAt(A, r, lead);
        if (leadval > 1) {
            Matrix_DivRow(A, r, leadval);
            Vector_DivRow(D, r, leadval);
        }

        for (int j = fromRow; j < toRow; j++) {
            if (j == r) {
                continue;
            }
            uint8_t multiple = Matrix_GetAt(A, j, lead);
            if (multiple != 0) {
                Matrix_XorRows(A, j, r, multiple);
                Vector_XorRows(D, j, r, multiple);
            }
        }

        lead++;
    }
    return true;
}

// in decoding process, the returned D2 may have M > L rows,
// so only its first L rows has intermedidate symbols !!
static void DecodingLastly(Matrix &A, Matrix &X, Vector &D, int i, int u,
    uint16_t c[], const Param &param, IAllocator *allocator)
{
    //printf("DecodingLastly, i = %d, u = %d\n", i, u);
    //DumpMatrix(A);
    //DumpMatrix(X);
    //DumpVector(D);

    // phase 3
    // by now, ony first i * i submatrix is useful in X, to multiple it with A
    // we have to zero the stride padding part
    int padding = CALCU_ALIGNMENT(i, 16) - i;
    if (padding > 0) {
        for (int r = 0; r < i; r++) {
            memset(X.data + X.rowStride * X.rowIdxes[r] + i, 0, padding);
        }
    }

    Matrix rotatedMat;
    rotatedMat.colNum = i;
    rotatedMat.rowNum = A.colNum;
    rotatedMat.rowStride = CALCU_ALIGNMENT(i, 16);
    rotatedMat.rowIdxes = nullptr; // not used
    rotatedMat.data = (uint8_t*)allocator->AllocFecBuffer(rotatedMat.rowStride * rotatedMat.rowNum, 16, IAllocator::PurposeTmpMatrix);
    Matrix_MulMatrix(A, X, i, rotatedMat);
    allocator->FreeFecBuffer(rotatedMat.data, IAllocator::PurposeTmpMatrix);

    // Now fix D, too
    Vector rotatedVec;
    rotatedVec.rowNum = D.rowSize;
    rotatedVec.rowSize = i;
    rotatedVec.rowStride = CALCU_ALIGNMENT(i, 16);
    rotatedVec.rowIdxes = nullptr;
    rotatedVec.data = (uint8_t*)allocator->AllocFecBuffer(rotatedVec.rowStride * rotatedVec.rowNum, 16, IAllocator::PurposeTmpMatrix);
    Vector_MulMatrix(D, X, i, rotatedVec);
    allocator->FreeFecBuffer(rotatedVec.data, IAllocator::PurposeTmpMatrix);

    // phase 4
    for (int j = 0; j < i; j++) {
        for (int k = param.L - u; k < param.L; k++) {
            uint8_t val = Matrix_GetAt(A, j, k);
            if (val != 0) {
                // U_upper is never read again, so we can avoid some writes
                // U_upper (row, col) = 0;
                // "b times row j of I_u" => row "j" in U_lower.
                // aka: U_upper.rows() + j
                Vector_XorRows(D, j, i + k - (param.L - u), val);
            }
        }
    }

    // phase 5
    for (int j = 0; j < i; j++) {
        uint8_t val = Matrix_GetAt(A, j, j);
        if (val > 1) {
            Matrix_DivRow(A, j, val);
            Vector_DivRow(D, j, val);
        }

        for (int k = 0; k < j; k++) {
            val = Matrix_GetAt(A, j, k);
            if (val != 0) {
                Vector_XorRows(D, j, k, val);
            }
        }
    }

    // finally, reuse D2 to take the C vector
    int *tmpIdx = (int*)allocator->AllocFecBuffer(D.rowNum * sizeof(int), 0, IAllocator::PurposeUnknown);
    Vector_Reorder(D, c, param.L, tmpIdx);
    allocator->FreeFecBuffer(tmpIdx, IAllocator::PurposeUnknown);
}

// decoding is used by encoder and decoder, to create the C from A and D
// there are 5 phases here to generate the immediate symbols
// if we want to determine the decoding okay, just put a dummy D with symbol size 1 here
bool Decoding(Matrix &A, Vector &D, const Param &param, IAllocator *allocator)
{
    assert(A.colNum == param.L);
    assert(A.rowNum == D.rowNum);
    assert(D.rowNum >= param.L);

    // decoding need a c and d vector
    // A is [L, L], D is [L, 1] -> A * D is [L * 1]
    uint16_t *c = (uint16_t*)allocator->AllocFecBuffer(param.L * sizeof(uint16_t), 0, IAllocator::PurposeUnknown);
    for (int k = 0; k < param.L; k++) {
        c[k] = k;
    }

    // clone X
    Matrix X = A;
    uint8_t *bufferOfX = (uint8_t*)allocator->AllocFecBuffer(
        A.rowNum * A.rowStride + CALCU_ALIGNMENT(A.rowNum * sizeof(int), 16), 16, IAllocator::PurposeTmpMatrix);
    X.rowIdxes = (int*)bufferOfX;
    X.data = bufferOfX + CALCU_ALIGNMENT(A.rowNum * sizeof(int), 16);
    memcpy(X.rowIdxes, A.rowIdxes, A.rowNum * sizeof(int));
    memcpy(X.data, A.data, A.rowNum * A.rowStride);

    bool success;
    int i, u;
    std::tie(success, i, u) = DecodingFirstly(A, X, D, c, param, allocator);
    if (success) {
        success = DecodingSecondly(A, D, i, u, param);
        if (success) {
            DecodingLastly(A, X, D, i, u, c, param, allocator);
        }
    }

    allocator->FreeFecBuffer(c, IAllocator::PurposeUnknown);
    allocator->FreeFecBuffer(bufferOfX, IAllocator::PurposeTmpMatrix);
    return success;
}

