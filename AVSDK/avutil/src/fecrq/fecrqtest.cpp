
//#include "stdafx.h"
#include "fecrq.h"
#include <stdlib.h>
#include <datetime.h>
//#include <Windows.h>

void fecRQTest2()
{
    uint8_t data[10];
    for (int i = 0; i < 10; i++)
        data[i] = i + 1;

    FECRQEncoder encoder(data, 10, 1);
    encoder.ForceEncoding();

    uint8_t tmp[10];
    for (int i = 0; i < 10; i++)
    {
        encoder.GetEncodedSymbols(i, &tmp[i]);
    }
    return;
}

void fecRQTest()
{
    srand(MediaCloud::Common::DateTime::TickCount());
    //srand(GetTickCount());
    uint32_t r = rand();
    int length = 1024 * 5 + r % 1024;
    int symbolSize = 64;

    // prepare testing data
    uint8_t *data = new uint8_t[length];
    
    srand(r);
    uint32_t r2 = rand();
    uint8_t random = r2;
    uint8_t step = r % 37;
    for (int i = 0; i < length; i++)
    {
        data[i] = random;
        random += step;
    }

    FECRQEncoder encoder(data, length, symbolSize);
    encoder.ForceEncoding();

    // verify the first K encoded symbols
    std::vector<uint8_t*> encodedSymbols;
    int K = encoder.NumOfSourceSymbols();
    for (int i = 0; i < 3 * K + 10; i++)
    {
        uint8_t *tmp = new uint8_t[symbolSize];
        encoder.GetEncodedSymbols(i, tmp);
        encodedSymbols.push_back(tmp);

        if (i < K)
        {
            int dataLen = i + 1 < K ? symbolSize : length - symbolSize * i;
            assert(dataLen > 0);
            if (memcmp(data + symbolSize * i, tmp, dataLen) != 0)
            {
                assert(false);
            }
        }
    }

    // testing decoding
    bool suc = false;
    std::vector<int> inputESIs;
    int eraseCnt = 0;

    FECRQDecoder decoder(symbolSize, K, 0);
    int inputcnt = 0;
    for (int i = 0; inputcnt <= K + 3; i++)
    {
        if (eraseCnt < K)
        {
            srand(r2);
            r2 = rand();
            if ((r2 % 2) == 0)
            {
                eraseCnt++;
                continue;
            }
        }

        inputcnt++;
        inputESIs.push_back(i);

        uint32_t tick = GetTickCount();
        bool ret = decoder.AddEncodedSymbols(i, encodedSymbols[i]);
        tick = GetTickCount() - tick;

        if (ret)
        {
            suc = true;

            printf(">> Decoding esis: ");
            for (auto esi : inputESIs)
                printf("%d ", esi);
            printf("<< Overhead = %d, Taking = %d ms\n", inputcnt - K, tick);
            for (int j = 0; j < K; j++)
            {
                const uint8_t *source = decoder.GetSourceSymbols(j);
                int dataLen = j + 1 < K ? symbolSize : length - symbolSize * j;
                assert(dataLen > 0);
                if (memcmp(data + symbolSize * j, source, dataLen) != 0)
                {
                    assert(false);
                }
            }
            break;
        }
    }
    assert(suc);

    for (auto &p : encodedSymbols)
        delete[] p;
    delete[] data;
}

