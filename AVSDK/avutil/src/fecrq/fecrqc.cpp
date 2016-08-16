
#include "fecrqm.h"
#include "fecrq.h"
#include <assert.h>
#include <vector>
#include <tuple>
#include <stdint.h>
#ifndef  UINT32_MAX
#define UINT32_MAX        4294967295U
#endif
class FECRQGraph
{
    std::vector<std::pair<int, int>> _connections;
    int _maxConnections;
    int _numOfConnections;
    int _maxSize;
    int _curSize;

    // find the root node of a sub component in this graph
    int Find(const int id) const
    {
        int tmp = id;
        while (_connections[tmp].second != tmp)
            tmp = _connections[tmp].second;
        return tmp;
    }

public:
    FECRQGraph(int maxSize)
    {
        _maxSize = maxSize;
        _curSize = 0;
        _maxConnections = _numOfConnections = 0;
        _connections.reserve(maxSize);
    }

    void Reset(int size)
    {
        assert(size > 0 && size <= _maxSize);
        _curSize = size;
        _maxConnections = _numOfConnections = 0;

        _connections.clear();
        for (int i = 0; i < size; i++)
            _connections.emplace_back(1, i);
    }

    int NumOfConnection() const { return _numOfConnections; }

    void Connect(int node_a, int node_b)
    {
        // initialize if the connection number is zero
        assert(node_a < node_b);
        assert(node_b < _curSize);

        _numOfConnections++;

        int rep_a = Find(node_a);
        int rep_b = Find(node_b);
        if (rep_a == rep_b)
        {
            // they already in the same component
            return;
        }

        _connections[rep_a] = { _connections[rep_a].first + _connections[rep_b].first, rep_a };
        _connections[rep_b] = _connections[rep_a];
        if (node_a != rep_a)
            _connections[node_a] = _connections[rep_a];
        if (node_b != rep_b)
            _connections[node_b] = _connections[rep_b];

        if (_maxConnections < _connections[rep_a].first)
            _maxConnections = _connections[rep_a].first;
    }

    bool IsMax(int id) const
    {
        assert(id < _curSize);
        return _maxConnections == _connections[Find(id)].first;
    }
};

// A and X is [M * L], D is [L * 1]
static std::tuple<bool, int, int> FECRQDecodingFirstly(FECRQMatrix &A, FECRQMatrix &X, FECRQVector &D,
    std::vector<int> &d, std::vector<int> &c, const FECRQParam::Param &param)
{
    int i = 0;
    int u = param.P;  // come from p
    int S = param.S;
    int H = param.H;
    int L = param.L;
    int M = A.Rows();
    
    // track hdpc rows and original degree of each row
    std::vector<std::pair<bool, uint32_t>> tracking;
    tracking.reserve(M);
    for (int j = 0; j < M; j++)
    {
        uint32_t degree = A.RowDegree(j, 0, L - u);
        bool isHDPC = j >= S && j < S + H;
        tracking.emplace_back(std::make_pair(isHDPC, degree));
    }

    int loopCnt = 0;

    // pair is row idx, the first idx of one column
    std::vector<std::pair<uint16_t, uint16_t>> rRows;
    rRows.reserve(M);

    // for determine the max size component in V graph
    FECRQGraph graph(L - u);

    while (i + u < L)
    {
        rRows.clear();
        graph.Reset(L - u - i);

        // find the row
        int r = M;
        bool twoOnes = false;
        for (int j = i; j < M; j++)
        {
            int idxOfOne1 = -1, idxOfOne2 = -1;
            int nonZeros = A.CountRow(j, i, L - u, idxOfOne1, idxOfOne2, r);
            if (nonZeros > r || nonZeros == 0)
                continue;

            if (nonZeros < r)
            {
                r = nonZeros;
                rRows.clear();
            }

            // noneZeros == r
            bool inGraph = false;
            if (r == 2)
            {
                if (idxOfOne1 >= 0 && idxOfOne2 >= 0)
                {
                    // 2 ones row
                    twoOnes = true;
                    if (!tracking[j].first)
                    {
                        // only the row which is in graph has valid second value of pair
                        graph.Connect(idxOfOne1 - i, idxOfOne2 - i);
                        rRows.push_back(std::make_pair((uint16_t)j, (uint16_t)idxOfOne1));
                        inGraph = true;
                    }
                }
            }

            if (!inGraph)
                rRows.push_back(std::make_pair((uint16_t)j, (uint16_t)L));
        }

        // If all entries of V are zero, then no row is chosen and decoding fails.
        if (r == M)
        {
            return std::make_tuple(false, 0, 0);	// failure
        }

        // choose the row
        int chosen = M;
        if (r != 2)
        {
            // If r != 2, then choose a row with exactly r nonzeros in V with
            // minimum original degree among all such rows, except that HDPC
            // rows should not be chosen until all non - HDPC rows have been
            int minRowIdx = M;
            uint32_t minRowDegree = UINT32_MAX;
            int minHDPCRowIdx = M;
            uint32_t minHDPCRowDegree = UINT32_MAX;

            for (auto &pair : rRows)
            {
                uint32_t degree = tracking[pair.first].second;
                if (tracking[pair.first].first)
                {
                    if (degree < minHDPCRowDegree)
                    {
                        minHDPCRowDegree = degree;
                        minHDPCRowIdx = pair.first;
                    }
                }
                else
                {
                    if (degree < minRowDegree)
                    {
                        minRowDegree = degree;
                        minRowIdx = pair.first;
                    }
                }
            }

            chosen = minRowIdx < M ? minRowIdx : minHDPCRowIdx;
        }
        else
        {
            // If r = 2 and there is a row with exactly 2 ones in V, then
            // choose any row with exactly 2 ones in V that is part of a
            // maximum size component in the graph described above that is
            // defined by V.
            // If r = 2 and there is no row with exactly 2 ones in V, then
            // choose any row with exactly 2 nonzeros in V.
            if (graph.NumOfConnection() > 0)
            {
                for (auto &pair : rRows)
                {
                    if (pair.second < L)
                    {
                        // this row is in graph
                        if (graph.IsMax(pair.second - i))
                        {
                            chosen = pair.first;
                            break;
                        }
                    }
                }
            }
            if (chosen == M)
                chosen = rRows[0].first;
        }

        assert(chosen >= i && chosen < M);

        // swap chosen row and first V row in A (not just in V)
        if (chosen != i)
        {
            A.SwapRow(i, chosen);
            X.SwapRow(i, chosen);
            std::swap(d[i], d[chosen]);
            std::swap(tracking[i], tracking[chosen]);
        }

        //The columns of A among those
        //that intersect V are reordered so that one of the r nonzeros in the
        //chosen row appears in the first column of V and so that the remaining
        //r - 1 nonzeros appear in the last columns of V.
        if (A.GetAt(i, i) == 0)
        {
            for (int j = i + 1; j < L - u; j++)
            {
                if (A.GetAt(i, j) != 0)
                {
                    A.SwapColumn(i, j);
                    X.SwapColumn(i, j);
                    std::swap(c[i], c[j]);
                    break;
                }
            }
        }
        int cntOfRestNonZero = r - 1; // there are this number of non-zero from i + 1 to L - u
        int cntOfTailNonZero = 0;
        for (int j = L - u - 1; j > i && cntOfRestNonZero > 0; j--)
        {
            if (A.GetAt(i, j) != 0)
            {
                int swapIdx = L - u - 1 - cntOfTailNonZero;
                if (j != swapIdx)
                {
                    A.SwapColumn(j, swapIdx);
                    X.SwapColumn(j, swapIdx);
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
        uint8_t alpha = A.GetAt(i, i);
        for (int j = i + 1; j < M; j++)
        {
            uint8_t beta = A.GetAt(j, i);
            if (beta == 0)
                continue;

            uint8_t divisor = FECOctetDiv(beta, alpha);
            A.AddInPlace(j, i, divisor);
            D.AddInPlace(d[j], d[i], divisor);
        }

        //printf("decoding phase1, loop %d, i is %d, u is %d, r is %d, twoones %d, chose %d\n", loopCnt, i, u, r, twoOnes, chosen);
        //A.Dump();
        //D.Dump(&d[0]);

        i++;
        u += r - 1;
        loopCnt++;
    }

    assert(i + u == L);
    return std::make_tuple(true, i, u);
}

// make a Gaussian elimination on A's U_lower sub matrix
static bool FECRQDecodingSecondly(FECRQMatrix &A, FECRQVector &D, int i, int u,
    std::vector<int> &d, const FECRQParam::Param &param)
{
    int fromRow = i, toRow = A.Rows();
    int toCol = param.L;
    int lead = param.L - u;
    for (int r = fromRow; r < toRow && lead < toCol; r++)
    {
        int chosen = r;
        while (A.GetAt(chosen, lead) == 0)
        {
            if (++chosen == toRow)
                break;
        }

        if (chosen == toRow)
        {
            // all zeros in this col, error
            // the final U_lower will not be u rank
            return false;
        }

        if (chosen != r)
        {
            A.SwapRow(r, chosen);
            std::swap(d[r], d[chosen]);
        }

        uint8_t leadval = A.GetAt(r, lead);
        if (leadval > 1)
        {
            A.DivRow(r, leadval);
            D.DivRow(d[r], leadval);
        }

        for (int j = fromRow; j < toRow; j++)
        {
            if (j == r)
                continue;

            uint8_t multiple = A.GetAt(j, lead);
            if (multiple != 0)
            {
                A.SubInPlace(j, r, multiple);
                D.SubInPlace(d[j], d[r], multiple);
            }
        }

        lead++;
    }

    //A.Dump();
    //D.Dump(&d[0]);
    return true;
}

// in decoding process, the returned D2 may have M > L rows,
// so only its first L rows has intermedidate symbols !!
static FECRQVector FECRQDecodingLastly(FECRQMatrix &A, FECRQMatrix &X, FECRQVector &D, int i, int u,
    std::vector<int> &c, std::vector<int> &d, const FECRQParam::Param &param)
{
    // phase 3
    A.MulMatrix(X, i);

    // Now fix D, too
    FECRQVector D2;
    D2.CloneFrom(D);
    D.MulMatrix(X, i, D2, &d[0]);

    //A.Dump();
    //D.Dump(&d[0]);

    // phase 4
    for (int j = 0; j < i; j++)
    {
        for (int k = param.L - u; k < param.L; k++)
        {
            uint8_t val = A.GetAt(j, k);
            if (val != 0)
            {
                // U_upper is never read again, so we can avoid some writes
                // U_upper (row, col) = 0;
                // "b times row j of I_u" => row "j" in U_lower.
                // aka: U_upper.rows() + j
                D.AddInPlace(d[j], d[i + k - (param.L - u)], val);
            }
        }
    }

    //A.Dump();
    //D.Dump(&d[0]);

    // phase 5
    for (int j = 0; j < i; j++)
    {
        uint8_t val = A.GetAt(j, j);
        if (val > 1)
        {
            A.DivRow(j, val);
            D.DivRow(d[j], val);
        }

        for (int k = 0; k < j; k++)
        {
            val = A.GetAt(j, k);
            if (val != 0)
                D.AddInPlace(d[j], d[k], val);
        }
    }

//    A.Dump();
//    D.Dump(&d[0]);

    // finally, reuse D2 to take the C vector
    for (int j = 0; j < param.L; j++)
        D2.CopyRow(c[j], D, d[j]);

    return D2;
}


// decoding is used by encoder and decoder, to create the C from A and D
// there are 5 phases here to generate the immediate symbols
// if we want to determine the decoding okay, just put a dummy D with symbol size 1 here
FECRQVector FECRQDecoding(FECRQMatrix &A, FECRQVector &D, const FECRQParam::Param &param)
{
    assert(A.Columns() == param.L);
    assert(A.Rows() == D.Rows());
    assert(D.Rows() >= param.L);

    // decoding need a c and d vector
    // A is [L, L], D is [L, 1] -> A * D is [L * 1]
    std::vector<int> c; c.reserve(param.L);
    for (int k = 0; k < param.L; k++) c.push_back(k);

    int M = D.Rows();
    std::vector<int> d; d.reserve(M);
    for (int k = 0; k < M; k++) d.push_back(k);

    bool success;
    int i, u;
    FECRQMatrix X;
    X.CloneFrom(A);

    std::tie(success, i, u) = FECRQDecodingFirstly(A, X, D, d, c, param);
    if (!success)
        return FECRQVector();

    success = FECRQDecodingSecondly(A, D, i, u, d, param);
    if (!success)
        return FECRQVector();

    return FECRQDecodingLastly(A, X, D, i, u, c, d, param);
}

