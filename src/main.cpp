#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

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
#include "commonstruct.h"
#include "Log.h"
#include "TcpChannel.h"
#include "BufSerialize.h"
#include "avmBizsChannel.h"
#include "avmGridChannel.h"
#include "rapidjson/document.h"
#include "cppcmn.h"


#define AVM_MAX_CONF_RECV_PACK_SIZE    1024*1024

using namespace AVMedia;
using namespace AVMedia::MediaIO;
using namespace MediaCloud::Adapter;
using namespace rapidjson;
using namespace MediaCloud;

void Help()
{
    printf("codecmix. Version %s\r\n", AVM_VERSION_STRING);
    printf("-h can be used to show this information\r\n");
    printf("example: ./codecmix xxx.conf\r\n");
    printf("Last Modify time is 2016-08-09 11:05.\r\n");
}

void AnalyzeIPAndPort(const char* pszSrc, VEC_PT_SERVERADDR& vecSvrAddr )
{
    char* pSemicolo = strstr((char*)pszSrc, ";"); 
    if(NULL==pSemicolo)
        return;
    char* pcolon = strstr(pSemicolo+1, ":");
    if(NULL==pcolon)
        return;

    PT_SERVERADDR pSvrAddr= new T_SERVERADDR;
    strncpy(pSvrAddr->szIP, pSemicolo+1, pcolon-pSemicolo-1);
    pSvrAddr->szIP[pcolon-pSemicolo-1]='\0';
    pSvrAddr->usPort = atoi(pcolon+1); 
    vecSvrAddr.push_back(pSvrAddr);
}

bool AnalyzeConfJson(char* pszJson)
{
    bool bRtn=false;
    Document doc;
    doc.Parse(pszJson);    
    
    assert(doc.HasMember("data"));
    string strJson = doc["data"].GetString();

    doc.Parse(strJson.c_str());
//    assert(doc.HasMember("clients"));

    const Value& valstClients = doc["clients"];
    assert(valstClients.IsArray());
    string strName("");
    string strType("");
    for (SizeType i = 0; i < valstClients.Size(); i++) 
    {
        const Value& vaClient = valstClients[i];
        vaClient.HasMember("name");
        if(!vaClient.HasMember("name") || !vaClient.HasMember("type"))
            continue;

        strName = vaClient["name"].GetString();
        strType = vaClient["type"].GetString();
        
        if(0!=strName.compare(g_confFile.strName) || 0!=strType.compare(g_confFile.strType))
            continue;

        g_confFile.tConfSvr.strName = strName;
        g_confFile.tConfSvr.strType = strType;
        const Value& csps = vaClient["csps"];
        for(SizeType j=0;j<csps.Size();j++)
        {
            string strcsps = csps[j].GetString();
            AnalyzeIPAndPort(strcsps.c_str(), g_confFile.tConfSvr.vecstrCSPS);
            //g_confFile.tConfSvr.vecstrCSPS.push_back(strcsps);
        }
        const Value& bizs=vaClient["bizs"];
         for(SizeType j=0;j<bizs.Size();j++)
        {
            string strbizs = bizs[j].GetString();
            AnalyzeIPAndPort(strbizs.c_str(), g_confFile.tConfSvr.vecstrbizs);
            //g_confFile.tConfSvr.vecstrbizs.push_back(strbizs);
        }      
    }

    bRtn=true;
    return bRtn;
}


int main(int argc, char** argv)
{
    int ret = 0;
    switch (argc)
    {
    case 1:
        Help();
        return 0;
        break; 
    case 2:
        if( !strcmp(argv[1], "-h") || !strcmp(argv[1],"-H") )
        {
            Help();
            return 0;
        }   
        break;
    default:
        Help();
        return 0;
        break; 
    }   
 
    if(!CCommonStruct::ReadConfig(argv[1]))
        return 0;
    
 
    string strLogFile = g_confFile.strLogDir;
    strLogFile += "codecmix.log";
    g_pLogHelper = open_logfile(strLogFile.c_str());
    set_log_level(g_pLogHelper, g_confFile.iLogLevel);

    log_info(g_pLogHelper, (char*)"main: codecmix start.........................");
    signal(SIGPIPE,SIG_IGN);

    //connect conf server, get config json
    CTCPChannel tcpChannel;
    tcpChannel.CreateChannel(NULL, 0);    
    if(0!=tcpChannel.Connect(g_confFile.pszConfSvrIP, g_confFile.usConfSvrPort))
    {
        log_err(g_pLogHelper,(char*)"connect config server failed. confserver:%s:%d", g_confFile.pszConfSvrIP, g_confFile.usConfSvrPort);
        return 0; 
    }
    
    string strReqJson=("{\"cmd\": \"data-request\", \"db-version\": 0}");    

    char szGetConfReq[100];
    char* pGCRCur=szGetConfReq;
    int iSendFact=0;
    pGCRCur = CBufSerialize::WriteUInt8(pGCRCur, 0Xfa);
    pGCRCur = CBufSerialize::WriteUInt8(pGCRCur, 0Xaf);
    pGCRCur = CBufSerialize::WriteUInt32_Net(pGCRCur, strReqJson.size());
    pGCRCur = CBufSerialize::WriteBuf(pGCRCur, strReqJson.c_str(), strReqJson.size());
    uint32_t uiReqPackLen=pGCRCur-szGetConfReq;
   
    iSendFact = tcpChannel.SendPacket(szGetConfReq, uiReqPackLen);
    if(0>=iSendFact)
    {
        log_err(g_pLogHelper, "send request to config server failed. confSer:%s:%d rtn:%d", g_confFile.pszConfSvrIP, g_confFile.usConfSvrPort, iSendFact);
        return 0;
    }

    char cType[2];
    uint32_t uijsonLen;
    int iRecvFact=0;
    iRecvFact = tcpChannel.RecvPacket((char*)&cType, 1);
    iRecvFact = tcpChannel.RecvPacket((char*)&cType+1, 1);
    if(0xfa!=(uint8_t)cType[0]||0xaf!=(uint8_t)cType[1])
    {
        log_err(g_pLogHelper,(char*)"config server responce is type failed. %d %d", (uint8_t)cType[0], (uint8_t)cType[1]);
        return 1;
    }
    
    iRecvFact = tcpChannel.RecvPacket((char*)&uijsonLen, sizeof(uijsonLen));
    uijsonLen = ntohl(uijsonLen);
    if(0>uijsonLen || AVM_MAX_CONF_RECV_PACK_SIZE<uijsonLen) 
    {
        log_err(g_pLogHelper,(char*)"config server responce is too long. confserver:%s:%d rsplen:%d", g_confFile.pszConfSvrIP, g_confFile.usConfSvrPort, uijsonLen);
        return 1;
    }    
   
    char* szGetConfRsp = new char[uijsonLen+1];
    uint32_t uiGetConfRspSize=uijsonLen;
    iRecvFact = tcpChannel.RecvPacket(szGetConfRsp, uiGetConfRspSize);
    if(iRecvFact!=uiGetConfRspSize)
    {
        log_err(g_pLogHelper,(char*)"recv responce json failed. needrecv:%d factrecv:%d", uijsonLen, iRecvFact);
        return 1;
    }
    szGetConfRsp[uijsonLen]='\0';
    log_info(g_pLogHelper,(char*)"recv responce json :%s.", szGetConfRsp);
    
    //analyze the response json 
    AnalyzeConfJson(szGetConfRsp);
    
    delete[] szGetConfRsp;
    szGetConfRsp=NULL;
    tcpChannel.DestoryChannel();

    //write json to log
    log_info(g_pLogHelper, (char*)"server conf json name:%s", g_confFile.tConfSvr.strName.c_str());
    log_info(g_pLogHelper, (char*)"server conf json type:%s", g_confFile.tConfSvr.strType.c_str());
    ITR_VEC_PT_SERVERADDR itr = g_confFile.tConfSvr.vecstrCSPS.begin();
    PT_SERVERADDR pSvrAddr=NULL;
    int iIndex=0;
    while(itr!=g_confFile.tConfSvr.vecstrCSPS.end())
    {
        pSvrAddr = *itr;
        log_info(g_pLogHelper, (char*)"server conf json csps_%d: %s:%d", iIndex++, pSvrAddr->szIP, pSvrAddr->usPort);
        itr++;
    } 
    itr= g_confFile.tConfSvr.vecstrbizs.begin();
    pSvrAddr=NULL;
    iIndex=0;
    while(itr!=g_confFile.tConfSvr.vecstrbizs.end())
    {
        pSvrAddr = *itr;
        log_info(g_pLogHelper, (char*)"server conf json bizs_%d: %s:%d", iIndex++, pSvrAddr->szIP, pSvrAddr->usPort);
        itr++;
    } 

    CAVMMixer::InitMixer();   
    cppcmn::InitializeCppcmn();

    //create grid channel to connect grid server
    CAVMGridChannel gridChannel;

    PT_SERVERADDR pGridAddr = g_confFile.tConfSvr.vecstrCSPS.front();
    if(NULL!=pGridAddr)
    {
        if(gridChannel.CreateAndConnect(pGridAddr->szIP, pGridAddr->usPort))
        {
            log_info(g_pLogHelper, (char*)"connect grid server successed. serverinfo:%s:%d", pGridAddr->szIP, pGridAddr->usPort);
            //gridChannel.Start();
        }
        else
        {
            log_err(g_pLogHelper, (char*)"connect grid server failed. serverinfo:%s:%d", pGridAddr->szIP, pGridAddr->usPort);
            return 0;
        }
    }

    //create bizs channel to connect bizs server
    CAVMBizsChannel bizChannel;
    PT_SERVERADDR pServerAddr = g_confFile.tConfSvr.vecstrbizs.front();
    bizChannel.SetGridChannel(&gridChannel);
    if(NULL!=pServerAddr)
    {
        if(bizChannel.CreateAndConnect(pServerAddr->szIP, pServerAddr->usPort))
        {
            log_info(g_pLogHelper, (char*)"connect bizs server successed. serverinfo:%s:%d", pServerAddr->szIP, pServerAddr->usPort );
            gridChannel.SetBizChannelObj(&bizChannel);
            gridChannel.Start();
            bizChannel.Start();
        }
        else
        {
            log_err(g_pLogHelper, (char*)"connect bizs server failed. serverinfo:%s:%d", pServerAddr->szIP, pServerAddr->usPort );
            return 0;
        }
    }

    while(1)
    {
        sleep(1);
    }
    bizChannel.DestoryChannel();
    gridChannel.DestoryChannel();
    return 1;
}
