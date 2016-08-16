//
//  Mp3FileIO.cpp
//  MediaIO
//
//  Created by douya on 16/4/16.
//  Copyright © 2016年 Wang Tao. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <avutil/include/datetime.h>
#include <avutil/include/common.h>

#include "Mp3FileIO.hpp"

using namespace AVMedia;
using namespace AVMedia::NetworkLayer;
using namespace AVMedia::NetworkLayer::Mp3FileIO;

namespace AVMedia {
    namespace NetworkLayer {
        namespace Mp3FileIO {
            //---------------------------------------------------------------------------
            const char* Mpega_Version[4]=
            {
                "MPA2.5",
                "",
                "MPA2",
                "MPA1"
            };
            
            //---------------------------------------------------------------------------
            const char* Mpega_Layer[4]=
            {
                "",
                "L3",
                "L2",
                "L1",
            };
            
            //---------------------------------------------------------------------------
            const char* Mpega_Format_Profile_Version[4]=
            {
                "Version 2.5",
                "",
                "Version 2",
                "Version 1"
            };
            
            //---------------------------------------------------------------------------
            const char* Mpega_Format_Profile_Layer[4]=
            {
                "",
                "Layer 3",
                "Layer 2",
                "Layer 1",
            };
            
            //---------------------------------------------------------------------------
            const unsigned short Mpega_BitRate[4][4][16]=
            {
                {{0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},  //MPEG Audio 2.5 layer X
                    {0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160,   0},  //MPEG Audio 2.5 layer 3
                    {0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160,   0},  //MPEG Audio 2.5 layer 2
                    {0,  32,  48,  56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256,   0}}, //MPEG Audio 2.5 layer 1
                {{0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},  //MPEG Audio X layer X
                    {0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},  //MPEG Audio X layer 3
                    {0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},  //MPEG Audio X layer 2
                    {0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}}, //MPEG Audio X layer 1
                {{0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},  //MPEG Audio 2 layer X
                    {0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160,   0},  //MPEG Audio 2 layer 3
                    {0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160,   0},  //MPEG Audio 2 layer 2
                    {0,  32,  48,  56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256,   0}}, //MPEG Audio 2 layer 1
                {{0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},  //MPEG Audio 1 layer X
                    {0,  32,  40,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320,   0},  //MPEG Audio 1 layer 3
                    {0,  32,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384,   0},  //MPEG Audio 1 layer 2
                    {0,  32,  64,  96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,   0}}, //MPEG Audio 1 layer 1
            };
            
            //---------------------------------------------------------------------------
            const unsigned short Mpega_SamplingRate[4][4]=
            {
                {11025, 12000,  8000, 0}, //MPEG Audio 2.5
                {    0,     0,     0, 0}, //MPEG Audio X
                {22050, 24000, 16000, 0}, //MPEG Audio 2
                {44100, 48000, 32000, 0}, //MPEG Audio 1
            };
            
            //---------------------------------------------------------------------------
            const unsigned short Mpega_Channels[4]=
            {
                2,
                2,
                2,
                1,
            };
            
            //---------------------------------------------------------------------------
            const char* Mpega_Codec_Profile[4]=
            {
                "",
                "Joint stereo",
                "Dual mono",
                "",
            };
            
            //---------------------------------------------------------------------------
            const char* Mpega_Codec_Profile_Extension[]=
            {
                "",
                "Intensity Stereo",
                "MS Stereo",
                "Intensity Stereo + MS Stereo",
            };
            
            //---------------------------------------------------------------------------
            const char* Mpega_Emphasis[]=
            {
                "",
                "50/15ms",
                "Reserved",
                "CCITT",
            };
            
            static unsigned int getbits(unsigned int x, int p, int n)
            {
                return (x >> (32-p-n)) & ~(~0<<n);
            }
            
            
            static unsigned int  getUpTo9bits(unsigned char *ptBitStream,
                                              int neededBits) /* number of bits to read from the bit stream 2 to 8 */
            {
                
                unsigned char Elem  = *(ptBitStream);
                unsigned char Elem1 = *(ptBitStream + 1);
                
                unsigned int returnValue = ((( unsigned int)(Elem)) << 8) |
                ((( unsigned int)(Elem1)) ) ;
                
                return (unsigned int)(returnValue >> (16 - neededBits));
            }
            
            CMp3FileReader::CMp3FileReader(AVPushPullType type)
            : IReader(type)
            , _csPull(new CriticalSection())
            , _cs(new CriticalSection())
            {
                m_strFileName  = NULL;
                m_f = NULL;
                m_revAudioSize = 0;
                m_nSeekTimeMs = -1;
                m_readerCallBack = NULL;
                m_readSize = 1024;
                m_pMp3FileInfo = new MediaFileInfo;
                
                m_data = (uint8_t*)malloc(m_readSize);
                
                m_AudioInfo.identity      = 100;
                m_AudioInfo.nCodec        = eMP3;
                m_AudioInfo.nStreamType   = eAudio;
                m_AudioInfo.isContinue   = true;
                m_AudioInfo.audio.nSampleRate = 44100;
                m_AudioInfo.audio.nChannel = 2;
                m_AudioInfo.audio.nBitPerSample = 16;
                
                _pullThread =GeneralThread::Create(PullThreadFun, this, false, normalPrio,"CMp3FileReader");
                
                Init();
            }
            
            CMp3FileReader::~CMp3FileReader()
            {
                UnInit();
                
                if(_pullThread) {
                    GeneralThread::Release(_pullThread);
                    _pullThread = NULL;
                }
                
                SafeDelete(m_data);
                SafeDelete(_csPull);
                SafeDelete(m_pMp3FileInfo);
            }
            
            bool CMp3FileReader::PullThreadFun(void* pParam)
            {
                CMp3FileReader*  pSelf = (CMp3FileReader*)  pParam;
                pSelf->pullData();
                
                return true;
            }
            
            bool CMp3FileReader::Init( )
            {
                m_bInit = true;
                m_bRun = false;
                
                printf("pull CMp3FileReader thread start\n");
                
                return true;
            }
            
            bool CMp3FileReader::UnInit()
            {
                m_bInit = false;
                
                if(m_f)
                {
                    fclose(m_f);
                    m_f = NULL;
                }
                
                return true;
            }
            
            int CMp3FileReader::Open(char* strFileName,IReaderCallBack *callBack, int nParam, long long nExtParam)
            {
                ScopedCriticalSection cs(_csPull);
                
                //    Close();
                
                m_bRun  = false;
                m_readerCallBack = callBack;
                
                m_readerCallBack->HandleData(NULL, 0, &m_AudioInfo);
                
                if(! m_bRun){
                    _pullThread->Start();
                    printf("start pull thread \n");
                }
                
                if(!strFileName )
                    return -1;
                
                m_strFileName = strFileName;
                //    _cv           = cv;
                
                // find mp3 sync byte;
                unsigned int   nSync;                    // 11bits
                unsigned char  ID = 0;                       // 2bits
                unsigned char  layer = 0;                    // 2bits
                unsigned char  protection_bit;           // 1bits
                unsigned char  bitrate_index = 0;            // 4bits
                unsigned char  sampling_frequency = 0;       // 2bits
                unsigned char  padding_bit;              // 1bits
                unsigned char  private_bit;              // 1bits
                unsigned char  mode = 0;                     // 2bits
                unsigned char  mode_extension;           // 2bits
                unsigned char  copyright;                // 1bits
                unsigned char  original_home;            // 1bits
                unsigned char  emphasis;                 // 2bits
                
                unsigned char Buf[4096] = {0};
                unsigned char buf1[5] = {0};
                unsigned char* pBuf = NULL;
                bool bFindSync = false;
                ID3 id3_buf;
                int nSize = 0;
                int ID3Size =0;
                int nRead = 0;
                m_f = fopen(strFileName, "rb");
                if(!m_f)
                    goto END;
                
                
                //get filesize
                fseek(m_f, 0, SEEK_END);
                nSize = (int)ftell( m_f );
                fseek(m_f, 0, SEEK_SET);
                
                //parse id3
                ID3Size = (int)fread(&id3_buf, 1, sizeof(id3_buf), m_f);
                if(ID3Size < (int)sizeof(id3_buf))
                    goto END;
                
                if(id3_buf.Header[0] == 'I' && id3_buf.Header[1] == 'D' && id3_buf.Header[2] == '3')
                {
                    int nPos = (id3_buf.Size[0]&0x7F);
                    nPos = (nPos << 7) + (id3_buf.Size[1]&0x7F);
                    nPos = (nPos << 7) + (id3_buf.Size[2]&0x7F);
                    nPos = (nPos << 7) + (id3_buf.Size[3]&0x7F);
                    fseek( m_f, nPos , SEEK_CUR);
                }
                else
                {
                    fseek( m_f, 0, SEEK_SET);
                }
                
                
                
                
                nRead = (int)fread(buf1, 1, 4, m_f);
                if(nRead < 4)   return -1;
                if (strncmp((char*)buf1, "RIFF", 4) == 0 || strncmp((char*)buf1, "RIFX", 4) == 0)
                {  }
                else
                    fseek(m_f, -4, SEEK_CUR);
                
                pBuf = Buf;
                while(1)
                {
                    int nRead = (int)fread(pBuf, 1, 4096 - (pBuf - Buf), m_f);
                    if(nRead <= 0)
                        goto END;
                    
                    
                    bFindSync = false;
                    int nPos = 0;
                    for(nPos = 0; nPos < (nRead - 4) ; ++nPos)
                    {
                        if(Buf[nPos] == 0xff && Buf[nPos + 1] == 0xff)
                            continue;
                        
                        if(getUpTo9bits(&Buf[nPos], 11) == 0x7ff)
                        {
                            int nBitPos = 0;
                            unsigned int nSncWord = Buf[nPos] << 24 | Buf[nPos + 1] << 16| Buf[nPos + 2] << 8 | Buf[nPos + 3] ;
                            nSync              = getbits(nSncWord, nBitPos, 11); nBitPos += 11;       //11bits
                            ID                 = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            layer              = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            protection_bit     = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            bitrate_index      = getbits(nSncWord, nBitPos, 4);  nBitPos += 4;        // 4bits
                            sampling_frequency = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            padding_bit        = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            private_bit        = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            mode               = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            mode_extension     = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            copyright          = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            original_home      = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            emphasis           = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;;       // 2bits
                            
                            bFindSync = true;
                            break;
                        }
                        
                    }
                    
                    if(bFindSync)
                    {
                        fseek(m_f, -(4096 - nPos), SEEK_CUR);
                        break;
                    }
                    else
                    {
                        int nRemain = nRead - nPos;
                        memcpy(&Buf, &Buf[nPos], nRemain);
                        pBuf = Buf + nRemain;
                    }
                }
                
                if(!bFindSync)
                    goto END;
                
                if(m_pMp3FileInfo)
                {
                    m_pMp3FileInfo->nCodec        = 4;
                    m_pMp3FileInfo->nSampleRate   = Mpega_SamplingRate[ID][sampling_frequency];
                    m_pMp3FileInfo->nChannels     = Mpega_Channels[mode];
                    m_pMp3FileInfo->nMaxBufSize   = 4096;
                    m_pMp3FileInfo->nFileSize     = nSize;
                    m_pMp3FileInfo->nFrameSize    = 4096;
                    m_pMp3FileInfo->nAddtional    = 0;
                    m_pMp3FileInfo->nDurationInMs = (nSize * 8/Mpega_BitRate[ID][layer][bitrate_index]);
                    m_pMp3FileInfo->ExtInfo.szLyrData = NULL;
                    m_pMp3FileInfo->ExtInfo.nLyrSize  = 0;
                    m_pMp3FileInfo->ExtInfo.szCardData= NULL;
                    m_pMp3FileInfo->ExtInfo.nCardSize= 0;
                }
                
                
                return 0;
                
            END:
                if(m_f)
                {
                    fclose(m_f);
                    m_f = NULL;
                }
                return -1;
                
            }
            
            int CMp3FileReader::Close()
            {
                LogDebug("Mp3FileReader", "stopReading");
                if ((_pullThread&&m_bRun)||(m_f==NULL)) {
                    m_bRun = false;
                    _pullThread->Stop();
                }else{
                    return false;
                }
                
                return true;
            }
            
            
            int CMp3FileReader::Seek(float percent)
            {
                ScopedCriticalSection cs(_csPull);
                return  1;
            }
            
            int CMp3FileReader::GetCurPos()
            {
                ScopedCriticalSection cs(_csPull);
                
                if(!m_f) return -1;
                
                return (int)ftell(m_f);
            }
            
            int  CMp3FileReader::Pause()
            {
                return 0;
            }
            
            int  CMp3FileReader::Resume()
            {
                return 0;
            }
            
            int CMp3FileReader::SetFileName(char* strFileName)
            {
                ScopedCriticalSection cs(_csPull);
                
                if(!strFileName){
                    return -1;
                }
                
                m_strFileName = strFileName;
                
                unsigned char Buf[4096] = {0};
                unsigned char buf1[5] = {0};
                unsigned char* pBuf = NULL;
                bool bFindSync = false;
                ID3 id3_buf;
                int nSize = 0;
                int ID3Size =0;
                int nRead = 0;
                
                // find mp3 sync byte;
                unsigned int   nSync;                    // 11bits
                unsigned char  ID=0;                       // 2bits
                unsigned char  layer = 0;                    // 2bits
                unsigned char  protection_bit;           // 1bits
                unsigned char  bitrate_index = 0;            // 4bits
                unsigned char  sampling_frequency = 0;       // 2bits
                unsigned char  padding_bit;              // 1bits
                unsigned char  private_bit;              // 1bits
                unsigned char  mode=0;                     // 2bits
                unsigned char  mode_extension;           // 2bits
                unsigned char  copyright;                // 1bits
                unsigned char  original_home;            // 1bits
                unsigned char  emphasis;                 // 2bits
                
                
                
                if(m_f)
                {
                    fclose(m_f);
                    m_f = NULL;
                }
                
                m_f = fopen(strFileName, "rb");
                if(!m_f)
                    goto END;
                
                //get filesize
                fseek(m_f, 0, SEEK_END);
                nSize = (int)ftell( m_f );
                fseek(m_f, 0, SEEK_SET);
                
                //parse id3
                ID3Size = (int)fread(&id3_buf, 1, sizeof(id3_buf), m_f);
                if(ID3Size < (int)sizeof(id3_buf))
                    goto END;
                
                if(id3_buf.Header[0] == 'I' && id3_buf.Header[1] == 'D' && id3_buf.Header[2] == '3')
                {
                    int nPos = (id3_buf.Size[0]&0x7F);
                    nPos = (nPos << 7) + (id3_buf.Size[1]&0x7F);
                    nPos = (nPos << 7) + (id3_buf.Size[2]&0x7F);
                    nPos = (nPos << 7) + (id3_buf.Size[3]&0x7F);
                    fseek( m_f, nPos , SEEK_CUR);
                }
                else
                {
                    fseek( m_f, 0, SEEK_SET);
                }
                
                
                nRead = (int)fread(buf1, 1, 4, m_f);
                if(nRead < 4)   return -1;
                if (strncmp((char*)buf1, "RIFF", 4) == 0 || strncmp((char*)buf1, "RIFX", 4) == 0)
                {  }
                else
                    fseek(m_f, -4, SEEK_CUR);
                
                pBuf = Buf;
                while(1)
                {
                    int nRead = (int)fread(pBuf, 1, 4096 - (pBuf - Buf), m_f);
                    if(nRead <= 0)
                        goto END;
                    
                    
                    bFindSync = false;
                    int nPos = 0;
                    for(nPos = 0; nPos < (nRead - 4) ; ++nPos)
                    {
                        if(Buf[nPos] == 0xff && Buf[nPos + 1] == 0xff)
                            continue;
                        
                        if(getUpTo9bits(&Buf[nPos], 11) == 0x7ff)
                        {
                            int nBitPos = 0;
                            unsigned int nSncWord = Buf[nPos] << 24 | Buf[nPos + 1] << 16| Buf[nPos + 2] << 8 | Buf[nPos + 3] ;
                            nSync              = getbits(nSncWord, nBitPos, 11); nBitPos += 11;       //11bits
                            ID                 = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            layer              = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            protection_bit     = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            bitrate_index      = getbits(nSncWord, nBitPos, 4);  nBitPos += 4;        // 4bits
                            sampling_frequency = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            padding_bit        = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            private_bit        = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            mode               = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            mode_extension     = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;        // 2bits
                            copyright          = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            original_home      = getbits(nSncWord, nBitPos, 1);  nBitPos += 1;        // 1bits
                            emphasis           = getbits(nSncWord, nBitPos, 2);  nBitPos += 2;;       // 2bits
                            
                            bFindSync = true;
                            break;
                        }
                        
                    }
                    
                    if(bFindSync)
                    {
                        fseek(m_f, -(4096 - nPos), SEEK_CUR);
                        break;
                    }
                    else
                    {
                        int nRemain = nRead - nPos;
                        memcpy(&Buf, &Buf[nPos], nRemain);
                        pBuf = Buf + nRemain;
                    }
                }
                
                if(!bFindSync)
                    goto END;
                
                if(m_pMp3FileInfo)
                {
                    m_pMp3FileInfo->nCodec        = 4;
                    m_pMp3FileInfo->nSampleRate   = Mpega_SamplingRate[ID][sampling_frequency];
                    m_pMp3FileInfo->nChannels     = Mpega_Channels[mode];
                    m_pMp3FileInfo->nMaxBufSize   = 4096;
                    m_pMp3FileInfo->nFileSize     = nSize;
                    m_pMp3FileInfo->nFrameSize    = 4096;
                    m_pMp3FileInfo->nAddtional    = 0;
                    m_pMp3FileInfo->nDurationInMs = (nSize * 8/Mpega_BitRate[ID][layer][bitrate_index]);
                    m_pMp3FileInfo->ExtInfo.szLyrData = NULL;
                    m_pMp3FileInfo->ExtInfo.nLyrSize  = 0;
                    m_pMp3FileInfo->ExtInfo.szCardData= NULL;
                    m_pMp3FileInfo->ExtInfo.nCardSize= 0;
                }
                
            END:
                if(m_f)
                {
                    fclose(m_f);
                    m_f = NULL;
                }
                return -1;
                
            }
            
            int  CMp3FileReader::pullData()
            {
                m_bRun = true;
                
                
                do {
                    if(! m_bInit){
                        break;
                    }
                    if(! m_bRun){
                        if(m_readerCallBack){
                            IOState iostate = {m_strFileName, StateClosed, Error_None};
                            m_readerCallBack->HandleReaderMessage(-1, 0, (long long)&iostate);
                        }
                        break;
                    }
                    
                    
                    {
                        ScopedCriticalSection cs(_csPull);
                        if(m_f)
                        {
                            Read(m_readSize);
                            
                        }
                        else{break;}
                    }
                    
                    ThreadSleep(20);
                }while(1);
                
                m_bRun = false;
                if(m_readerCallBack){
                    //        m_readerCallBack->HandleReaderStatus(ReportStatus, StateClosed,  Error_None);
                }
                return 0;
            }
            
            int CMp3FileReader::Read(unsigned char* pBuf, int nBufSize)
            {
                if(!m_f) return -1;
                
                if(m_nSeekTimeMs != -1)
                {
                    
                    m_nSeekTimeMs = -1;
                }
                
                
                size_t ret = fread(pBuf, 1, nBufSize, m_f);
                
                
                if(m_readerCallBack&&ret>0)
                {
                    uint8_t* audioBuf = (uint8_t*)malloc(nBufSize);
                    memcpy(audioBuf, pBuf, nBufSize);
                    m_readerCallBack->HandleData(audioBuf, nBufSize, &m_AudioInfo);
                    
                    m_revAudioSize++;
                    free(audioBuf);
                    audioBuf = NULL;
                }
                else if(ret<=0)
                {
                    fclose(m_f);
                    m_f = NULL;
                    m_bRun = false;
                }
                
                return (int)ret;
            }
            size_t CMp3FileReader::Read(int nBufSize)
            {
                if(!m_f) return -1;
                
                if(m_nSeekTimeMs != -1)
                {
                    
                    m_nSeekTimeMs = -1;
                }
                
                
                
                size_t ret = fread(m_data, 1, nBufSize, m_f);
                
                if(ret>0)
                {
                    uint8_t* audioBuf = (uint8_t*)malloc(m_readSize);
                    memcpy(audioBuf, m_data, m_readSize);
                    
                    m_buffs.push_back(audioBuf);
                    if(m_buffs.size()>3)
                        _bFull = true;
                    
                    if(_bFull)
                    {
                        HandleData();
                    }
                }
                else if(ret<=0)
                {
                    fclose(m_f);
                    m_f = NULL;
                    m_bRun = false;
                }
                
                return ret;
            }
            void CMp3FileReader::HandleData()
            {
                if(m_readerCallBack!=NULL)
                {
                    
                    uint8_t* audioBuf = m_buffs.front();
                    m_buffs.pop_front();
                    if(m_buffs.size()<=3)
                        _bFull = false;
                    
                    m_readerCallBack->HandleData(audioBuf, m_readSize, &m_AudioInfo);
                    
                    free(audioBuf);
                    audioBuf = NULL;
                }
                
            }
            
            int CMp3FileReader::HandleData(long param, long long nExtParam)
            {
                return 0;
            }
            
            int CMp3FileReader::HandleMessage(long param, long long nExtParam)
            {
                return 0;
            }
        }
    }
}

