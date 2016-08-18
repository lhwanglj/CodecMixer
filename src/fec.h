#ifndef _HPSP_FEC_H
#define _HPSP_FEC_H

#include <stdint.h>
#include "logging.h"
#include "fbuffer.h"
#include "fast_fec.h"

using namespace MediaCloud::Common;

namespace hpsp {

    class FecBufferAllocator : public fec::IAllocator {
    public:
        // actually, fast-fec only support max matrix size = 233 for 200 src symbols
        static const int MaxL = 256;

        FecBufferAllocator()
            // size = max matrix size + matrix column index buffer
            : _tmpMatrixAllocator(MaxL * MaxL + MaxL * sizeof(int), 4, 16)
            , _tmpUnknownAllocator(MaxL * 8, 4, 0)
            // lt symbol buffer has column index at the begin
            , _ltAllocator(MaxL * (MaxL + 4), 1, 16)
        {
        }

        virtual void* AllocFecBuffer(int size, int alignment, int purpose) override
        {
            if (purpose == PurposeLT) {
                Assert(size <= _ltAllocator.AlignedSize());
                return _ltAllocator.Alloc();
            }

            if (purpose == PurposeTmpMatrix) {
                Assert(size <= _tmpMatrixAllocator.AlignedSize());
                return _tmpMatrixAllocator.Alloc();
            }

            Assert(purpose == PurposeUnknown);
            Assert(size <= _tmpUnknownAllocator.AlignedSize());
            return _tmpUnknownAllocator.Alloc();
        }

        virtual void FreeFecBuffer(void *buffer, int purpose) override
        {
            if (buffer != nullptr) {
                if (purpose == PurposeLT) {
                    _ltAllocator.Free(buffer);
                }
                else if (purpose == PurposeTmpMatrix) {
                    _tmpMatrixAllocator.Free(buffer);
                }
                else {
                    Assert(purpose == PurposeUnknown);
                    _tmpUnknownAllocator.Free(buffer);
                }
            }
        }

        cppcmn::FixedSizeAllocator _tmpMatrixAllocator;
        cppcmn::FixedSizeAllocator _tmpUnknownAllocator;
        cppcmn::FixedSizeAllocator _ltAllocator;
    };
}
#endif

