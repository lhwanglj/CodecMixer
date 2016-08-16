//
//  IMediaIO.cpp
//  rtmptool
//
//  Created by xingyanjun on 16/4/15.
//  Copyright © 2016年 jiangyuanfu. All rights reserved.
//

#include "IMediaIO.h"
#include "net/NetTransfer.h"
#include "File/Mp3FileIO.hpp"
#include "net/SocketHandle.h"
//#include "hpsp/hpsp.h"

namespace AVMedia {
    namespace MediaIO
    {
     /*   static  MComp::HPSP* g_hpsp = NULL;
        MComp::HPSP* getHpspInstance() {
            if(g_hpsp == NULL)
                g_hpsp = MComp::HPSP::Create();
            return g_hpsp;
        }*/
        ISocketHandle* CreateSocketHandle(const char* url, AVPushPullType type, ISocketCallBack* callBack)
        {
            ISocketHandle * pHander = NULL;
            
            size_t urllen = strlen(url);
            if(urllen < 7){
                return NULL;
            }
            
            char protocol[8] = {0};
            memcpy(protocol, url, 7);
            
            for(int i = 0; i < 4; i++){
                protocol[i] = ::tolower(protocol[i]);
            }
            
            if(strstr(protocol, "rtmp://") != NULL){
                pHander = new AVMedia::NetworkLayer::RTMPProtocol::RTMPSocketHandle(type, callBack);
            }
            else  if(strstr(protocol, "http://") != NULL){
                if(AVPushPullType_backplay == type){
              //      pHander = new AVMedia::NetworkLayer::HTTPRePlay::HTTPRePlaySocketHandle(type, callBack);
                    pHander = new AVMedia::NetworkLayer::HTTPProtocol::HTTPVodSocketHandle(type, callBack);
                }else{
                    pHander = new AVMedia::NetworkLayer::HTTPProtocol::HTTPSocketHandle(type, callBack);
                }
            }
            
            return pHander;
        }
        
        void     DestroySocketHandle(ISocketHandle** pHander)
        {
            if(*pHander)
            {
                (*pHander)->DisConnect();
                delete (*pHander);
                *pHander = NULL;
            }
        }
        
        IReader* CreateReader(const char* url, AVPushPullType type)
        {
            IReader * pReader = NULL;
            
            size_t urllen = strlen(url);
            if(urllen < 6){
                return NULL;
            }
            
            char protocol[8] = {0};
            memcpy(protocol, url, 7);
            
            for(int i = 0; i < 4; i++){
                protocol[i] = ::tolower(protocol[i]);
            }
            
            if(strstr(protocol, "rtmp://") != NULL){
                pReader = new AVMedia::NetworkLayer::RTMPProtcol::NetReader(type);
            }
            else  if(strstr(protocol, "http://") != NULL){
                pReader = new AVMedia::NetworkLayer::RTMPProtcol::NetReader(type);
            }else  if(strstr(protocol, "hpsp://") != NULL){
               // pReader = getHpspInstance()->GetReader();
            }
            else if(strstr(protocol, "mp3") != NULL){
                //mp3
                pReader = new AVMedia::NetworkLayer::Mp3FileIO::CMp3FileReader(type);
            }
            else{
                if(urllen >= 4){
                    char suffix[6] = {0};
                    memcpy(suffix, url + urllen - 4, 4);
                    if(strstr(suffix, ".flv") != NULL){
             //           pReader = new AVMedia::NetworkLayer::HTTPFlv::httpFlvReader();
                    }else if(strstr(suffix, ".mp4") != NULL || strstr(suffix, ".m4a") != NULL){
                    }else if(strstr(suffix, "mp3") != NULL){
                        pReader = new AVMedia::NetworkLayer::Mp3FileIO::CMp3FileReader(type);
                    }
                }
            }
            
            return pReader;
        }
        
        void     DestroyReader(IReader** pReader)
        {
            if(*pReader)
            {
               // (*pReader)->Close();
                delete (*pReader);
                *pReader = NULL;
            }
        }
        
        IWriter* CreateWriter(const char* url)
        {
            IWriter* pWriter = NULL;
            
            size_t urllen = strlen(url);
            if(urllen < 7){
                return NULL;
            }
            
            char protocol[8] = {0};
            memcpy(protocol, url, 7);
            
            for(int i = 0; i < 4; i++){
                protocol[i] = ::tolower(protocol[i]);
            }
            
            if(strstr(protocol, "rtmp://") != NULL){
                pWriter = new AVMedia::NetworkLayer::RTMPProtcol::RtmpWriter();
            }
            else  if(strstr(protocol, "http://") != NULL){
                if(urllen >= 11){
                    char suffix[4];
                    memcpy(suffix, url + urllen - 4, 4);
                    if(strstr(suffix, ".flv") != NULL){
                    }else if(strstr(suffix, ".mp4") != NULL || strstr(suffix, ".m4a") != NULL){
                    }else if(strstr(suffix, "mp3") != NULL){
                    }
                }
            } else  if(strstr(protocol, "hpsp://") != NULL){
               // pWriter = getHpspInstance()->GetWriter();
            }else{
                if(urllen >= 4){
                    char suffix[6] = {0};
                    memcpy(suffix, url + urllen - 4, 4);
                    if(strstr(suffix, ".flv") != NULL){
                    }else if(strstr(suffix, ".mp4") != NULL || strstr(suffix, ".m4a") != NULL){
                    }else if(strstr(suffix, "mp3") != NULL){
                    }
                }
            }
            
            return pWriter;
        }
        
        void     DestroyWriter(IWriter** pWriter)
        {
            if(*pWriter)
            {
                (*pWriter)->Close();
                delete (*pWriter);
                *pWriter = NULL;
            }
        }
    }
}