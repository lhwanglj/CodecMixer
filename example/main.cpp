#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#include <MediaIO/IMediaIO.h>
#include <MediaIO/net/srs_librtmp.h>
#include <audiomixer.h>


#include <wavreader.h>
#include <i420reader.h>
#include <i420writer.h>
#include <audiocodec.h>
#include <videocodec.h>
#include <videofilter.h>
#include <picturemixer.hpp>


//#define SEND_RTMP_BY_SRS_LIBRTMP

#define MMIX_MAX_VIDEO_BUF_LEN    1024 
//#define MMIX_MAX_VIDEO_BUF_LEN    8192 
#define MMIX_MAX_AUDIO_BUF_LEN    500

#define MMIX_MAX_LOOP_COUNT    100000

#define FRAME_RATE 16 
#define VIDEO_W    480 
#define VIDEO_H    642 

using namespace AVMedia;
using namespace AVMedia::MediaIO;
using namespace MediaCloud::Adapter;

void pushvideo(IWriter* pRtmpWriter, u_int32_t* ptimestamp, char* pData, int iSize)
{
    MediaInfo mVInfo;
    mVInfo.identity = 1;
    mVInfo.nStreamType = eVideo;
    mVInfo.nCodec = eH264;
    mVInfo.isContinue = true;
    mVInfo.video.nWidth = 480;
    mVInfo.video.nHeight =360;
    mVInfo.video.nDtsTimeStamp=*ptimestamp;
    mVInfo.video.nDtsTimeStampOffset=0;
    mVInfo.video.nFrameRate = 15;
    
    char cftype = srs_utils_flv_video_frame_type(pData, iSize);
    srs_human_trace("cur frame type:%d. strtype:%s", cftype, srs_human_flv_video_frame_type2string(cftype));
    
    if(1 == cftype)
        mVInfo.video.nType = eIFrame;
    else
        mVInfo.video.nType = eBFrame ;

    pRtmpWriter->Write( (unsigned char*)pData, iSize, &mVInfo);
}


void pushvideoEx(IWriter* pRtmpWriter, VideoEncodedData * pVEData)
{
    if(NULL == pVEData)
        return;

    unsigned char * pData = (unsigned char *)pVEData->iData;
    unsigned int nSize = pVEData->iDataLen;
    u_int32_t timestamp=pVEData->iDts;
    printf("pushvideoEx pus data....  len:%d dts:%d pts:%d\r\n", nSize, pVEData->iDts, pVEData->iPts);

#ifndef SEND_RTMP_BY_SRS_LIBRTMP
   
    H264VideoFrameType h264 = pVEData->iFrameType.h264;
    EnumFrameType frameType;
    if(kVideoSPSFrame == h264){
        frameType = eSPSFrame;
    }else if(kVideoPPSFrame == h264){
        frameType = ePPSFrame;
    }else if(kVideoIDRFrame == h264){
        frameType = eIDRFrame;
    }else if(kVideoIFrame == h264){
        frameType = eIFrame;
    }else if(kVideoPFrame == h264){
        frameType = ePFrame;
    }else if(kVideoBFrame == h264){
        frameType = eBFrame;
    }else{
        return;
    }

    MediaInfo mVInfo;
    mVInfo.identity = 1;
    mVInfo.nStreamType = eVideo;
    mVInfo.nCodec = eH264;
    mVInfo.frameId = 0;
    mVInfo.video.nType      = frameType;
    mVInfo.video.nWidth = VIDEO_W;
    mVInfo.video.nHeight =VIDEO_H;
    mVInfo.video.nFrameRate=FRAME_RATE;
    mVInfo.video.nDtsTimeStamp=pVEData->iDts;
    mVInfo.video.nDtsTimeStampOffset=pVEData->iPts-pVEData->iDts;

    pRtmpWriter->Write( pData, nSize, &mVInfo);

#else
    if ((ret = srs_human_print_rtmp_packet(9, timestamp, pData, nSize)) != 0) {
        srs_human_trace("print packet failed. ret=%d timestamp%d", ret, timestamp);
    }
      
    if ((ret = srs_rtmp_write_packet(ortmp, 9, timestamp, pData, nSize)) != 0) {
        srs_human_trace("irtmp get packet failed. ret=%d", ret);
    }
#endif
}

void pushaudio(IWriter* pRtmpWriter, u_int32_t* ptimestamp, char* pData, int iSize)
{

    MediaInfo mAInfo;
    mAInfo.identity = 1;
    mAInfo.nStreamType = eAudio;
    mAInfo.nCodec      = eAAC;
    mAInfo.nProfile    = 1;// unknow 
    mAInfo.isContinue = true;
    mAInfo.audio.nSampleRate = 44100;
    mAInfo.audio.nChannel = 2;
    mAInfo.audio.nBitPerSample=16;
    mAInfo.audio.nTimeStamp = *ptimestamp;
        
    pRtmpWriter->Write( (unsigned char*)pData, iSize, &mAInfo);
}


void* memmem(void* start, unsigned int s_len, void* find, unsigned int f_len)
{
    char* p = (char *) start;
    char* q = (char *) find;
    unsigned int len = 0;
    
    while(s_len >= (p - (char *) start + f_len)){
        while(*p++ == *q++){
            len++;
            if(len == f_len){
                return (p - f_len);
            }
        }
        q = (char *) find;
        len = 0;
    }
    
    return(NULL);
}

int nal_length1( const unsigned char * pData, int nLen,int& prefix)
{
    int len;
    
    assert(pData[0] == 0);
    assert(pData[1] == 0);
    
    if(pData[2] == 1){
        prefix = 3;
    }else if(pData[3] == 1){
        if(pData[2] != 0){
            assert(false);
            return -1;
        }
        prefix = 4;
    }else{
        assert(false);
        return -1;
    }
    
    unsigned char* next = (unsigned char *) memmem(
                                                   &pData[prefix], nLen - prefix, &pData[0], prefix);
    if(next != NULL){
        len = (int) (next - &pData[0]);
    }else{
        len = nLen;
    }
    
    return len;
}

void VideoMix(unsigned char* pSrcData, unsigned int iSrcDataSize, int iSrcWidth, int iSrcHeight, 
                unsigned char* pDstData, unsigned int iDstDataSize, int iDstWidth, int iDstHeight, int iDstStrideY, int iDstStrideUV)
{
    int iSpaceW = 30;
    int iSpaceH = 40;
    MergeMainFrameInfo MMFI;
    MMFI.nFormat = 0;
    MMFI.nFrameWidth = iSrcWidth;
    MMFI.nFrameHeight = iSrcHeight;        
    MMFI.pData = pSrcData;
    MMFI.nLength = iSrcDataSize;

    unsigned char* pTemp =  (unsigned char*)malloc(iDstWidth*iDstHeight);
    memset(pTemp, 255, iDstWidth*iDstHeight);

    MergeOtherFrameInfo mofi[3];
    mofi[0].OrigType = kMediaLibraryOrigRightTop;// kMediaLibraryOrigLeftTop;
    mofi[0].nImgW = iDstWidth;
    mofi[0].nImgH = iDstHeight;
    mofi[0].nOffsetX = iSpaceW;
    mofi[0].nOffsetY = iSpaceH;
    mofi[0].pData[0] = pDstData;
    mofi[0].pData[1] = pDstData+iDstStrideY*iDstHeight;
    mofi[0].pData[2] = pDstData+iDstStrideY*iDstHeight+iDstStrideUV*iDstHeight/2;
    mofi[0].pData[3] =pTemp; 
   
    mofi[1].OrigType = kMediaLibraryOrigRightTop;// kMediaLibraryOrigLeftTop;
    mofi[1].nImgW = iDstWidth;
    mofi[1].nImgH = iDstHeight;
    mofi[1].nOffsetX = iSpaceW;
    mofi[1].nOffsetY = iSpaceH*2+iDstHeight;
    mofi[1].pData[0] = pDstData;
    mofi[1].pData[1] = pDstData+iDstStrideY*iDstHeight;
    mofi[1].pData[2] = pDstData+iDstStrideY*iDstHeight+iDstStrideUV*iDstHeight/2;
    mofi[1].pData[3] =pTemp; 
 
    mofi[2].OrigType = kMediaLibraryOrigRightTop;// kMediaLibraryOrigLeftTop;
    mofi[2].nImgW = iDstWidth;
    mofi[2].nImgH = iDstHeight;
    mofi[2].nOffsetX = iSpaceW;
    mofi[2].nOffsetY = (iSpaceH+iDstHeight)*2+iSpaceH;
    mofi[2].pData[0] = pDstData;
    mofi[2].pData[1] = pDstData+iDstStrideY*iDstHeight;
    mofi[2].pData[2] = pDstData+iDstStrideY*iDstHeight+iDstStrideUV*iDstHeight/2;
    mofi[2].pData[3] =pTemp; 
 
    CPictureMixer::MergeFrame(MMFI, mofi, 3); 
}

void YUV2H264( CVideoCodec* pVEncoder, unsigned char* pData, unsigned int uiSize, int iWidth, int iHeight, int iRate, VideoEncodedList* pVEList  )
{
    static int nFrame = 0;
    FrameDesc frameDesc;
    frameDesc.iFrameType.h264 = kVideoUnknowFrame;
    frameDesc.iPreFramesMiss = false;
    frameDesc.iPts           = nFrame * 1000/16;
            
    memset(pVEList, 0, sizeof(VideoEncodedList));
    pVEncoder->Process(pData, uiSize, &frameDesc, pVEList);
    nFrame++;
    usleep(50000);
}

void* h2642rtmp()
{
    InitMediaCommon(NULL, NULL, NULL, NULL);
    
    char* h264file = (char*)"./test.h264";
    char* pRtmpUrl = (char*)"rtmp://101.201.146.134/hulu/w_test.flv";     
    int ret = 0;
    u_int32_t timestamp=0;
    
#ifndef SEND_RTMP_BY_SRS_LIBRTMP
    IWriter* pRtmpWriter = CreateWriter(pRtmpUrl);
    pRtmpWriter->Open(pRtmpUrl, NULL); 
#else
    srs_rtmp_t ortmp = srs_rtmp_create(pRtmpUrl, &bRunningFlag);

    if ((ret = srs_rtmp_handshake(ortmp)) != 0) 
    {
        srs_human_trace("ortmp simple handshake failed. ret=%d", ret);
        return ret;
    }
    srs_human_trace("ortmp simple handshake success");

    if ((ret = srs_rtmp_connect_app(ortmp)) != 0) 
    {
        srs_human_trace("ortmp connect vhost/app failed. ret=%d", ret);
        return ret;
    }
    srs_human_trace("ortmp connect vhost/app success");

    if ((ret = srs_rtmp_publish_stream(ortmp)) != 0) 
    {
        srs_human_trace("ortmp publish stream failed. ret=%d", ret);
        return ret;
    }
    srs_human_trace("ortmp publish stream success");

#endif

    
 
    CVideoCodec* pVideoEncoder = CreateVideoCodec(kVideoCodecRAWH264, kEncoder);
    CVideoCodec* pVideoEncoderH264 = CreateVideoCodec(kVideoCodecH264, kEncoder);
    CVideoCodec* pVideoDecoder = CreateVideoCodec(kVideoCodecH264, kDecoder); 

    I420Writer i420writer;
    i420writer.Open("./test.yuv");
    
    CVideoFilter* pConvert = NULL; 
    pConvert = CreateVideoFilter(kVideoFilterImageConvert);
    if(NULL == pConvert)
    {
        printf("create video filter for image convert failed.\r\n");
        return NULL;
    }
    pConvert->Init(NULL);

 
    if (pVideoEncoder == NULL || NULL == pVideoDecoder) {
        printf("pVideoEncoder = NULL or pVideoDecoder = NULL\n");
        return NULL;
    }
    
    VideoCodecParam encParam;
    memset(&encParam, 0, sizeof(VideoCodecParam));
    encParam.encoder.iPicFormat  = kCodecPictureRawH264;
    encParam.encoder.iFrameRate  = FRAME_RATE;
    encParam.encoder.iWidth      = VIDEO_W;
    encParam.encoder.iHeight     = VIDEO_H;
    encParam.encoder.iMaxBitrate = 150;
    encParam.encoder.iStartBitrate = 150;
    encParam.encoder.iMinBitrate   = 50;
    encParam.encoder.qpMax         = 56;
    encParam.encoder.iProfile      = kComplexityNormal;
    encParam.feedbackModeOn        = false;
    encParam.numberofcores         = 2;
   
    VideoCodecParam encParamYUV;
    memset(&encParamYUV, 0, sizeof(VideoCodecParam));
    encParamYUV.encoder.iPicFormat  = kCodecPictureFmtI420;
    encParamYUV.encoder.iFrameRate  = FRAME_RATE;
    encParamYUV.encoder.iWidth      = VIDEO_W;
    encParamYUV.encoder.iHeight     = VIDEO_H;
    encParamYUV.encoder.iMaxBitrate = 650;
    encParamYUV.encoder.iStartBitrate = 650;
    encParamYUV.encoder.iMinBitrate   = 50;
    encParamYUV.encoder.qpMax         = 56;
    encParamYUV.encoder.iProfile      = kComplexityNormal;
    encParamYUV.feedbackModeOn        = false;
    encParamYUV.numberofcores         = 2;

    
    VideoCodecParam decParam;
    memset(&decParam, 0, sizeof(VideoCodecParam));
    
    decParam.feedbackModeOn        = false;
    if(!pVideoEncoder->Init(&encParam))
    {
        pVideoEncoder->DeInit();
    }
    if(!pVideoEncoderH264->Init(&encParamYUV))
    {
        pVideoEncoder->DeInit();
        pVideoEncoderH264->DeInit();
    }
    if(!pVideoDecoder->Init(&decParam))
    {
        pVideoEncoder->DeInit();
        pVideoEncoderH264->DeInit();
        pVideoDecoder->DeInit();
    }

    //----
   
    struct timeval tv1;
    struct timeval tv2;
 
    FILE* f = fopen(h264file, "rb");
    
    int frameLen = 100* 1024;
    uint8_t* frameBuffer= new uint8_t[frameLen];
    memset(frameBuffer, 0, frameLen);
    int nFrame = 0;
    int TotalByte    = 0;
    int count = 0;
    int pos = 0;
    do
    {
        int n = fread(frameBuffer + pos,1,frameLen-pos,f);
        printf("read file pos:%d n:%d. frameLen:%d\r\n", pos, n, frameLen);
        if(n <=0)
        {
            //wlj test
            break;

            fseek(f,0,SEEK_SET);
            printf("file end:\r\n");
            n=0;
            pos=0;
            continue;
        }
        n +=pos ; 
        pos = 0;

        int prefix;
        
        int nal_len = nal_length1(frameBuffer + pos, n, prefix);
        //printf("nal len:%d\r\n", nal_len);

        while(nal_len != n ){
            FrameDesc frameDesc;
            frameDesc.iFrameType.h264 = kVideoUnknowFrame;
            frameDesc.iPreFramesMiss = false;
            frameDesc.iPts           = nFrame * 1000/20;
            
            VideoEncodedList encoderList;
            memset(&encoderList, 0, sizeof(VideoEncodedList));
            pVideoEncoder->Process(frameBuffer + pos, nal_len , &frameDesc, &encoderList);
            
            for (int i = 0; i < encoderList.iSize;  ++i)
            {
                frameDesc.iFrameType = encoderList.iPicData[i].iFrameType;
               // printf("nal len:%d pos:%d frametype:%d\r\n", nal_len, pos, frameDesc.iFrameType);
                unsigned char* pData = (unsigned char*)encoderList.iPicData[i].iData;
                int iSize =  encoderList.iPicData[i].iDataLen;
                
                PictureData pic;
                memset(&pic, 0, sizeof(PictureData));
                
                pVideoDecoder->Process((const unsigned char*)pData, iSize, &frameDesc, &pic);


                if(0<pic.iPlaneData)
                {

                    printf("pic info fmt:%d s1:%d s2:%d s3:%d s4:%d po1:%d po2:%d po3:%d po4:%d w:%d h:%d size:%d tt:%d\r\n",
                                pic.iFormat, pic.iStrides[0], pic.iStrides[1],pic.iStrides[2],pic.iStrides[3],
                                pic.iPlaneOffset[0], pic.iPlaneOffset[1], pic.iPlaneOffset[2], pic.iPlaneOffset[3],
                                pic.iWidth, pic.iHeight, pic.iPlaneDataSize, pic.timestamp ); 
                    

                    //convert (s:512 w:480 h:640 size=s*1.5*h) to s:128: w:120 h:80
                    PictureData picDst;
                    int iDstWidth=120;
                    int iDstHeight=160;
                    int iDstStrideY = 120;
                    int iDstStrideUV = 60;
                    int iDstDataSize = iDstStrideY*iDstHeight*3/2;
                    unsigned char* pDstData = (unsigned char*)malloc(iDstDataSize);
                    
                    int iSrcWidth = pic.iWidth;
                    int iSrcHeight = pic.iHeight;
                    int iSrcStrideY = iSrcWidth;
                    int iSrcStrideUV = iSrcStrideY/2;
                    int iSrcDataSize = iSrcWidth*iSrcHeight*3/2;
                    unsigned char* pSrcData = (unsigned char*)malloc(iSrcDataSize); 
                    int iPosCur=0;
                    int i=0;
                    for(i=0;i<iSrcHeight; i++) 
                    {
                        memcpy(pSrcData+iPosCur, (unsigned char*)pic.iPlaneData+i*pic.iStrides[0], iSrcWidth);
                        iPosCur+=iSrcWidth;
                    }
                     for(i=0;i<iSrcHeight; i++) 
                    {
                        memcpy(pSrcData+iPosCur, (unsigned char*)pic.iPlaneData+pic.iStrides[0]*iSrcHeight+i*pic.iStrides[1], iSrcStrideUV);
                        iPosCur+=(iSrcStrideUV);
                    }

                    ImageConvertContext context;
                    context.imageSrc.format = kCodecPictureFmtI420; 
                    context.imageSrc.width = iSrcWidth;
                    context.imageSrc.height = iSrcHeight;
                    context.imageSrc.stride[0] = iSrcStrideY;
                    context.imageSrc.stride[1] = iSrcStrideUV;
                    context.imageSrc.stride[2] = iSrcStrideUV;
                    context.imageSrc.stride[3] = 0;
                    context.imageSrc.plane[0]  = (((unsigned char*)(pSrcData)));
                    context.imageSrc.plane[1]  = (((unsigned char*)(pSrcData))+iSrcWidth*iSrcHeight);
                    context.imageSrc.plane[2]  = (((unsigned char*)(pSrcData))+(iSrcWidth*iSrcHeight+iSrcStrideUV*iSrcHeight/2));
               //     context.imageSrc.plane[0]  = (((unsigned char*)(pic.iPlaneData))+pic.iPlaneOffset[0]);
               //     context.imageSrc.plane[1]  = (((unsigned char*)(pic.iPlaneData))+pic.iPlaneOffset[1]);
               //     context.imageSrc.plane[2]  = (((unsigned char*)(pic.iPlaneData))+pic.iPlaneOffset[2]);
                    context.imageSrc.plane[3]  = NULL;
                    context.imageDst.format = kCodecPictureFmtI420;               
                    context.imageDst.width    = iDstWidth;
                    context.imageDst.height   = iDstHeight;
                    context.imageDst.stride[0] = iDstStrideY;
                    context.imageDst.stride[1] = iDstStrideUV;
                    context.imageDst.stride[2] = iDstStrideUV;
                    context.imageDst.stride[3] = 0;
                    context.imageDst.plane[0]  = pDstData;
                    context.imageDst.plane[1]  = pDstData+iDstStrideY*iDstHeight;
                    context.imageDst.plane[2]  = pDstData+iDstStrideY*iDstHeight+iDstStrideUV*iDstHeight/2;
                    context.imageDst.plane[3]  = NULL;
               
                    gettimeofday(&tv1, NULL); 
                    pConvert->Process(&context);    
                    gettimeofday(&tv2, NULL);
                    printf("convert pic srcw:%d srch:%d dstw:%d dsth:%d used:%dus.\r\n", iSrcWidth, iSrcHeight, iDstWidth, iDstHeight, ((tv2.tv_sec*1000*1000+tv2.tv_usec)-(tv1.tv_sec*1000*1000+tv1.tv_usec))); 
 
                    gettimeofday(&tv1, NULL); 
                    VideoMix(pSrcData, iSrcDataSize, iSrcWidth, iSrcHeight, pDstData, iDstDataSize, iDstWidth, iDstHeight, iDstStrideY, iDstStrideUV);
                    gettimeofday(&tv2, NULL);
                    printf("mix pic srcw:%d srch:%d dstw:%d dsth:%d dstcount:3 used:%dus.\r\n", iSrcWidth, iSrcHeight, iDstWidth, iDstHeight, ((tv2.tv_sec*1000*1000+tv2.tv_usec)-(tv1.tv_sec*1000*1000+tv1.tv_usec))); 
        
                    VideoEncodedList velist;
                    //encode yuv data to h264 data
                    YUV2H264(pVideoEncoderH264, pSrcData, iSrcDataSize, iSrcWidth, iSrcHeight, 16, &velist );    
                    
                    printf("yuv2h264  datasize:%d w:%d h:%d listsize:%d\r\n", iSrcDataSize, iSrcWidth, iSrcHeight, velist.iSize); 
                    //send h264 data to rtmp server
                    for(int ii=0;ii<velist.iSize;ii++)     
                    {
                        pushvideoEx(pRtmpWriter,velist.iPicData+ii); 
                        free(velist.iPicData[ii].iData);
                    }

                    //write yuv data to file
                    //i420writer.Write((const char*)pSrcData, iSrcDataSize);
                    //i420writer.Write((const char*)pDstData, iDstDataSize);
                    //i420writer.Write((const char*)pic.iPlaneData, pic.iPlaneDataSize);
                    free(pic.iPlaneData);
                }
      
                //wlj 
               // pushvideoEx(pRtmpWriter, encoderList.iPicData+i);
               // free(encoderList.iPicData[i].iData);
                TotalByte += encoderList.iPicData[i].iDataLen;
            }
            
            free(encoderList.iPicData);
            
            nFrame++;
            usleep(5*1000);
            
            pos += nal_len;
            n -= nal_len;
            nal_len = nal_length1(frameBuffer + pos, n, prefix);
            
        }
        
        memmove(frameBuffer, frameBuffer + pos, n);
        pos=n; 
        
        
    } while (count++ < 1000);
    i420writer.Close(); 
    
    if(pVideoEncoder)
    {
        ReleaseVideoCodec(pVideoEncoder);
    }
    if(NULL != pVideoDecoder) 
        ReleaseVideoCodec(pVideoDecoder);

    fclose(f);
    
    
    return NULL;
    
}


int flv2rtmp()
{
    srs_flv_t flv;
    int ret = 0;
    flv = srs_flv_open_read("./test.flv");
    if(NULL == flv)
    {
        srs_human_trace("open flv file failed.");
        return 2;
    } 
    
    char header[13];
    if ((ret = srs_flv_read_header(flv, header)) != 0) 
    {
        srs_human_trace("flv read header failed.");
        return ret;
    }   

    u_int32_t timestamp=0;
    char type=0;
    int size=0;
    char* data = NULL;
    char* pRtmpUrl = (char*)"rtmp://101.201.146.134/hulu/w_test.flv";
    bool bRunningFlag=true;

#ifndef SEND_RTMP_BY_SRS_LIBRTMP
    IWriter* pRtmpWriter = CreateWriter(pRtmpUrl);
    pRtmpWriter->Open(pRtmpUrl, NULL); 
#else
    srs_rtmp_t ortmp = srs_rtmp_create(pRtmpUrl, &bRunningFlag);

    if ((ret = srs_rtmp_handshake(ortmp)) != 0) 
    {
        srs_human_trace("ortmp simple handshake failed. ret=%d", ret);
        return ret;
    }
    srs_human_trace("ortmp simple handshake success");

    if ((ret = srs_rtmp_connect_app(ortmp)) != 0) 
    {
        srs_human_trace("ortmp connect vhost/app failed. ret=%d", ret);
        return ret;
    }
    srs_human_trace("ortmp connect vhost/app success");

    if ((ret = srs_rtmp_publish_stream(ortmp)) != 0) 
    {
        srs_human_trace("ortmp publish stream failed. ret=%d", ret);
        return ret;
    }
    srs_human_trace("ortmp publish stream success");

#endif
    
    for(;;)
    {
       // tag header
        if((ret = srs_flv_read_tag_header(flv, &type, &size, &timestamp)) != 0) 
        {
            if (srs_flv_is_eof(ret)) 
            {
                srs_human_trace("parse completed.");
                return 0;
            }
            srs_human_trace("flv get packet failed. ret=%d", ret);
            return ret;
        }
        srs_human_trace("read tag header typd:%d  size:%d timestamp:%d. ", type, size, timestamp);
        if (size <= 0) 
        {
             srs_human_trace("invalid size=%d", size);
             break;
        }
        
        data = (char*)malloc(size);
        if ((ret = srs_flv_read_tag_data(flv, data, size)) != 0) 
        {
            return ret;
        }
        
        if ((ret = srs_human_print_rtmp_packet(type, timestamp, data, size)) != 0) 
        {
            srs_human_trace("print packet failed. ret=%d timestamp%d", ret, timestamp);
            return ret;
        }

#ifndef SEND_RTMP_BY_SRS_LIBRTMP
        if(8 == type)
            pushaudio(pRtmpWriter, &timestamp, data, size); 
        else if(9 == type)
            pushvideo(pRtmpWriter, &timestamp, data, size);

        free(data);
        data=NULL;
        usleep(20*1000);
#else
        if ((ret = srs_human_print_rtmp_packet(type, timestamp, data, size)) != 0) {
            srs_human_trace("print packet failed. ret=%d timestamp%d", ret, timestamp);
            return ret;
        }
        
      
        if ((ret = srs_rtmp_write_packet(ortmp, type, timestamp, data, size)) != 0) {
            srs_human_trace("irtmp get packet failed. ret=%d", ret);
            return ret;
        }
        usleep(20*1000);

#endif
    }

    srs_flv_close(flv);

}


//mix two wav file to a wav file
void test_mixwave()
{
    InitMediaCommon(NULL, NULL, NULL, NULL);
    
    CAudioCodec* pAudioEncoder = CreateAudioCodec(kAudioCodecFDKAAC, kEncoder);
    if(NULL == pAudioEncoder)
    {
        printf("create audio decoder failed. codec type: kAudioCodecFDKAAC.\r\n");
        return;
    }
    AudioCodecParam audioFormat;
    audioFormat.iSampleRate = 44100;
    audioFormat.iBitsOfSample=16;
    audioFormat.iNumOfChannels = 2;
    audioFormat.iQuality = 5;
    audioFormat.iProfile = 29;
    audioFormat.iHaveAdts = 1;

    audioFormat.ExtParam.iUsevbr = true;
    audioFormat.ExtParam.iUsedtx = true;
    audioFormat.ExtParam.iCvbr = true;
    audioFormat.ExtParam.iUseInbandfec = true;
    audioFormat.ExtParam.iPacketlossperc = 25;
    audioFormat.ExtParam.iComplexity = 8;
    audioFormat.ExtParam.iFrameDuration = 20*10;

    if(NULL == pAudioEncoder->Init(&audioFormat))
    {
        printf("init audio decoder failed.\r\n"); 
        return;
    }
    int iEncOutSize=0;
    pAudioEncoder->CalcBufSize(&iEncOutSize, 0);
    unsigned char* pAudioEncBuf = (unsigned char*)malloc(iEncOutSize);
       

    char* pRtmpUrl = (char*)"rtmp://101.201.146.134/hulu/w_test.flv";
    int ret = 0;
    int iAudioFramePos=0;
    u_int32_t timestamp=0;    

    IWriter* pRtmpWriter = CreateWriter(pRtmpUrl);
    pRtmpWriter->Open(pRtmpUrl, NULL);

    int input_size = 2*2*2048;

    AudioStreamFormat asf;
    asf.flag = 0;
    asf.sampleRate = 44100;
    asf.sampleBits= 16;
    asf.channelNum = 2;
    int iAudioTS = 0;
    double iAudioFrameBufLen = (double)asf.sampleRate * asf.channelNum * asf.sampleBits /8/1000;
    double dAudioFrameTSLen = (double)input_size/iAudioFrameBufLen;

    AudioMixer* pAMix =  AudioMixer::CreateAudioMixer(asf, 2); 

    //open wav file
    int format, sample_rate, channels, bits_per_sample;
    
    uint8_t* input_buf_1   = (uint8_t*) malloc(input_size);
    int16_t* convert_buf_1 = (int16_t*) malloc(input_size);
    uint8_t* input_buf_2   = (uint8_t*) malloc(input_size);
    int16_t* convert_buf_2 = (int16_t*) malloc(input_size);
    uint8_t* output_buf    = (uint8_t*) malloc(input_size);

    AudioMixer::AudioDataInfo adInfo[2];
    

    const char* pwavfilename1="input1.wav";
    const char* pwavfilename2="input2.wav";
    
    void *wav1= wav_read_open(pwavfilename1);
    void *wav2= wav_read_open(pwavfilename2);
    FILE *pfOutWav = fopen("output.pcm", "wb");;

 
    if (NULL != wav1) 
    {
        printf("open wav file %s ok\n", pwavfilename1);
        if (!wav_get_header(wav1, &format, &channels, &sample_rate, &bits_per_sample, NULL)) {
            return ;
        }
        if (format != 1 || bits_per_sample != 16) {
            printf("Unsupported WAV format %d\n", format);
            return ;
        }
    }    
    
    if (NULL != wav2) 
    {
        printf("open wav file %s ok\n", pwavfilename1);
        if (!wav_get_header(wav2, &format, &channels, &sample_rate, &bits_per_sample, NULL)) {
            return ;
        }
        if (format != 1 || bits_per_sample != 16) {
            printf("Unsupported WAV format %d\n", format);
            return ;
        }
    } 

    int read1 = 0;
    int read2 = 0;
    if(wav1)
    {
        //this data is for offset
        read1 = wav_read_data(wav1, input_buf_1, input_size);
        read1 = wav_read_data(wav1, input_buf_1, input_size);
        read1 = wav_read_data(wav1, input_buf_1, input_size);
        read1 = wav_read_data(wav1, input_buf_1, input_size);
        read1 = wav_read_data(wav1, input_buf_1, input_size);
    }

    struct timeval tv1;
    struct timeval tv2;

    //read wav file and mix wav file
    while (1) {
        if(wav1)
            read1 = wav_read_data(wav1, input_buf_1, input_size);
        if(wav1 && read1 <= 0)
        {
            break;
        }
        else if(read1 <=0 ){
            memset(input_buf_1,0,input_size);
        }

/*
        //not use is ok
        for (int i = 0; i < read1/2; i++) {
            const uint8_t* in = &input_buf_1[2*i];
            convert_buf_1[i] = in[0] | (in[1] << 8);
        }
*/

        if(wav2)
            read2 = wav_read_data(wav2, input_buf_2, input_size);
        if(wav2 && read2 <= 0)
            break;
        else if(read2 <=0 ){
            memset(input_buf_2,0,input_size);
        }
/*
        //not use is ok
        for (int i = 0; i < read2/2; i++) {
            const uint8_t* in = &input_buf_2[2*i];
            convert_buf_2[i] = in[0] | (in[1] << 8);
        }
*/

        adInfo[0]._bufferSize = adInfo[0]._leftLength = input_size;
        adInfo[0]._leftData = input_buf_1;
        adInfo[0]._enabled = true;

        adInfo[1]._bufferSize = adInfo[1]._leftLength = input_size;
        adInfo[1]._leftData = input_buf_2;
        adInfo[1]._enabled = true;
        int packetLen = channels*bits_per_sample/8;
        int packnum = input_size/packetLen;

        gettimeofday(&tv1, NULL); 
        pAMix->MixData(output_buf, packnum, packetLen, 0, adInfo, 2);
        gettimeofday(&tv2, NULL);
        
        printf("mix wav file len:%d used:%dus.\r\n", input_size, ((tv2.tv_sec*1000*1000+tv2.tv_usec)-(tv1.tv_sec*1000*1000+tv1.tv_usec))); 
        //printf("mix data read1:%d read2:%d input_size:%d\r\n", read1, read2, input_size);
    
        //write output wav file
   //   fwrite(output_buf, 1, input_size, pfOutWav);


        //encoder to fdkaac
        for (int i = 0; i < input_size/2; i++) {
            const uint8_t* in = &output_buf[2*i];
            convert_buf_1[i] = in[0] | (in[1] << 8);
        }

        int iAudioEncBufLen = iEncOutSize;
        pAudioEncoder->Process((unsigned char*)convert_buf_1, input_size, pAudioEncBuf, &iAudioEncBufLen);

        //send it to rtmp server
        timestamp = iAudioFramePos * dAudioFrameTSLen;
        pushaudio(pRtmpWriter, &timestamp, (char*)convert_buf_1, iAudioEncBufLen); 
        printf("push audio framePos:%d  wavlen:%d timestamp:%d enclen:%d  frametslen:%f\r\n", iAudioFramePos, input_size, timestamp, iAudioEncBufLen, dAudioFrameTSLen);
        iAudioFramePos++; 
        usleep(40*1000);    
    }

    if(wav1)
        wav_read_close(wav1); 
    if(wav2)
        wav_read_close(wav2); 
    if(pfOutWav)
        fclose(pfOutWav);


}

int main(int argc, char** argv)
{
    printf("hello world.\r\n");
   
    //test mix two wav files
    //test_mixwave();
    
    //test h264 file to rtmp file
    h2642rtmp();


    return 1;
}
