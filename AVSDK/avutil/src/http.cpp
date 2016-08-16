
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <common.h>
#define HTTPPROTO "HTTP/1.1"
#define HTTPPROTOLEN 8
#define HTTPFLAG "\r\n"			//每行数据结束
#define HTTPFLAGLEN 2
#define HTTPFLAGEND "\r\n\r\n"	//连续两个代表信息结束，后面是数据内容
#define HTTPFLAGENDLEN 4
#define CONTENT_LENGTH "Content-Length"
#define TRANSFER_ENCODING "Transfer-Encoding"
#define LOCATION "Location"
#define CHUNKED "chunked"
#define Content_Range "Content-Range"

using namespace MediaCloud::Common;

// url is the path part of http url
HttpRequest::HttpRequest(const char *url, const char *hostname, Method method, unsigned int timeout)
{
    _url = url;
    _hostname = hostname;
    _method = method;
    _timeout = timeout;
    _socket = NULL;

    _status = HttpNone;
    _checkhttp = true;
    _fileSize = 0;
    _receiveFilelength=0;//文件总长
    _chuncksize=0;//分片大小
    _countchunksize=0;//累计分片字节数
    _usechunk=0;//是否使用分片
}

HttpRequest::~HttpRequest()
{
    CompleteRequest(0);
    Assert(_status == HttpNone || _status == HttpClosed);
    Assert(_socket == NULL);
}

HttpRequest* HttpRequest::Create(const char *url, Method method, unsigned int timeout)
{
    if (url == NULL || *url == 0 || method != MethodGet)
        return NULL;

    if (timeout == 0)
        timeout = 10 * 1000;
    
    // appending default port to url
    char path[128] = { 0 };
    char hostname[128] = { 0 };
    char protoname[128] = { 0 };
    char newurl[200] = {0};
    unsigned short port = 0;
    const char *protocol = NULL;
    
    protocol = strchr(url, ' ');
    if(protocol != NULL){
        memcpy(newurl, url, protocol - url);
    }else{
        strcpy(newurl, url);
    }

    protocol = strchr(newurl, ':');
    if(protocol != NULL) {
        memcpy(protoname, newurl, protocol - newurl);
        if(strcmp(protoname, "http") == 0 && strlen(protocol) >=6) {
            if(protocol[0] == ':' && protocol[1] == '/' && protocol[2] == '/')
                protocol += 3;
            else
                return NULL;
        }
    }else {
        protocol = newurl;
    }
  
    const char *splash = strchr(protocol, '/');
    if (splash != NULL)
    {
        memcpy(hostname, protocol, splash - protocol);
        int hnlen = AsyncSocket::SplitHostnameAndPort(hostname, port);
        if (hnlen <= 0)
            strcat(hostname, ":80");
        
        strcpy(path, splash);
    }
    else
    {
        strcpy(hostname, protocol);
        if (port == 0)
        {
            strcat(hostname, ":80");
        }
        strcpy(path, "/");
    }

    return new HttpRequest(path, hostname, method, timeout);
}

int HttpRequest::Connect(bool *socket_running_flag)
{
    if (_status != HttpNone)
        return ErrCodeAlready;

    LogDebug("http", "starting http request to %s, path %s\n", _hostname.c_str(), _url.c_str());
    _socket = AsyncSocket::Create(this, AsyncSocket::SocketTypeTCP, 0,0);
	_socket->SetRunningFlag(socket_running_flag);

    int ret = _socket->Connect(_hostname.c_str());
    if (ret != ErrCodeNone)
    {
        LogError("http", "connecting failed to %s", _hostname.c_str());
        AsyncSocket::Release(_socket);
        _socket = NULL;
        return ret;
    }

    _status = HttpConnecting;
    return ret;
}

int HttpRequest::Begin(Method httpMethod, char * param){
    if(httpMethod  == MethodGet) //only Get
        return (SendRequest(param) ? 0 : -1);
    else
        return -1;
}

int HttpRequest::End(){
    if(_socket){
        _socket->Close();
    }
    return 0;
}

bool HttpRequest::GetIP(char *& localIP, char *& remoteIP)
{
    if(_socket){
        MediaCloud::Common::IPEndPoint localEP;
        MediaCloud::Common::IPEndPoint remoteEP;
        _socket->GetEndPoint(&localEP, &remoteEP);
        
        if(localEP.IsValid()){
            std::string ip = localEP.ToStringIP();
            memcpy(localIP, ip.c_str(), ip.length());
            localIP[ip.length()] = 0x00;
        }
        
        if(remoteEP.IsValid()){
            std::string ip = remoteEP.ToStringIP();
            memcpy(remoteIP, ip.c_str(), ip.length());
            remoteIP[ip.length()] = 0x00;
        }
        
        return  localEP.IsValid() && remoteEP.IsValid();
    }
    
    return false;
}

int  HttpRequest::ReadResponseData(char ** buffer, int & len, ResponseInfo & resInfo)
{
    len = 0;
    int res = -1;
    if(!_socket){
        return res;
    }
    
    const char *datastartpos = _recvBuffer;
    int ret = _socket->Recv(_recvBuffer, HTTPSTREAMRECVLEN);
    if(ret <= 0){
        return res;
    }
    
    if(_checkhttp) {
        char * nParam = NULL;
        unsigned int nParamLen = 0;
        _receiveFilelength = 0;
        _fileSize = 0;
        int statusCode = ParseHeader(_recvBuffer, ret, &datastartpos, nParam, nParamLen, resInfo.rangeStart, resInfo.rangeEnd);
        if(200 == statusCode || 206 == statusCode){
            int realLen = ret - (int)(datastartpos - _recvBuffer);
            if(_usechunk) {
                realLen = GetChunk(datastartpos, realLen, &_chuncksize, &_countchunksize);
                *buffer = _chunckBuffer;
            }else {
                *buffer = (char*)datastartpos;
            }
            len = realLen;
            
            resInfo.content_length = _receiveFilelength;
            resInfo.content_fileSize = _fileSize;
        }else if(301 == statusCode || 302 == statusCode){
            len = 0;
            if(nParam){
                len = nParamLen;
                *buffer = nParam;
            }
        }
        res = statusCode;
    }
    
    return res;
}

int HttpRequest::ReadData(char **buffer, ResponseInfo & resInfo){
    int res = -1;
    if(!_socket){
        return res;
    }
    
    const char *datastartpos = _recvBuffer;
    int ret = _socket->Recv(_recvBuffer, HTTPSTREAMRECVLEN);
    if(ret <= 0){
        return res;
    }
    
    if(_checkhttp) {
        char * nParam = NULL;
        unsigned int nParamLen = 0;
        _receiveFilelength = 0;
        _fileSize = 0;
        int statusCode = ParseHeader(_recvBuffer, ret, &datastartpos, nParam, nParamLen, resInfo.rangeStart, resInfo.rangeEnd);
        if(200 == statusCode){
            int realLen = ret - (int)(datastartpos - _recvBuffer);
            if(_usechunk) {
                realLen = GetChunk(datastartpos, realLen, &_chuncksize, &_countchunksize);
                *buffer = _chunckBuffer;
            }else {
                *buffer = (char*)datastartpos;
            }
            res = realLen;
            
            resInfo.content_length = _receiveFilelength;
            resInfo.content_fileSize = _fileSize;
        }else if(301 == statusCode || 302 == statusCode){
            res = 0;
            if(nParam){
                res = nParamLen;
                *buffer = nParam;
            }
        }
    }else {
        int realLen = ret - (int)(datastartpos - _recvBuffer);
        if(_usechunk) {
            realLen = GetChunk(datastartpos, realLen, &_chuncksize, &_countchunksize);
            *buffer = _chunckBuffer;
        }else {
            *buffer = (char*)datastartpos;
        }
        res = realLen;
    }
    
    return res;
}

int HttpRequest::ParseHeader(char buf[], int bufLeng, const char**datastartpos, char *& nParam, unsigned int & nParamLen, unsigned int & rangeStart, unsigned int & rangeEnd)
{
    nParam = NULL;
    nParamLen = 0;
    rangeStart = 0;
    rangeEnd = 0;
    int statusCode = -1;
    char key[50]= {0};
    char httpstat[20] = {0};
    int  statd = 0;
    sscanf(buf,"%s %d %s",key,&statd,httpstat);
    if (strncmp(key,"HTTP/1.0",strlen("HTTP/1.0"))!=0 && strncmp(key,"HTTP/1.1",strlen("HTTP/1.1"))!=0){
        printf("NOT HTTP\n");//不是HTTP会话
        return statusCode;
    }
    
    statusCode = statd;
    
//    if(strncmp(httpstat,"OK",2) == 0){
//    }else if(strncmp(httpstat,"Found",5) == 0){
//    }else{
//        
//    }
    
    const char * posend  = strstr(buf,HTTPFLAGEND);//二进制数据开始
    if (posend == NULL){
        printf("Can't Find ByteData\n");
        return false;
    }
    
    if(posend != NULL){
        *datastartpos = posend + HTTPFLAGENDLEN;
    }
    
    const char * pos2 = strstr(buf,HTTPFLAG);
    const char * pos1 = pos2+HTTPFLAGLEN;
    const char * posm = NULL;
    char value[200];
    
    while(1){
        pos2 = strstr(pos1,HTTPFLAG);
        posm = strstr(pos1,":");
        if (pos2==NULL||posm==NULL){
            break;
        }
        memset(key,0,50);
        memset(value,0,200);
        memcpy(key,pos1,posm-pos1);
        memcpy(value,posm+2,pos2-posm-2);//冒号，空格
        
        printf("%s: %s\n",key,value);
        if(strcmp(key, Content_Range) == 0){
            sscanf((const char*)value," bytes %d-%d/%d", &rangeStart, &rangeEnd, &_fileSize);
        }else if (strcmp(key,CONTENT_LENGTH) == 0){
            sscanf((const char*)value," %d",&_receiveFilelength);
        }else if(strcmp(key,LOCATION) == 0){
            if(301 == statusCode || 302 == statusCode){
//                if(nParam){
//                    free(nParam);
//                    nParam = NULL;
//                }
//                if(pos2-posm-2 > 0){
//                    nParam = (char *)malloc(pos2-posm-2 + 1);
//                    memcpy(nParam, posm+2, pos2-posm-2);
//                    nParam[pos2-posm-2] = 0x00;
//                }
                nParam = (char *)posm+2;
                nParamLen = (unsigned int) (pos2-posm-2);
            }
        }else if (strcmp(key,TRANSFER_ENCODING) == 0){
            if (strcmp(value,CHUNKED)==0){
                _usechunk=1;
            }else{
                printf("Warnning UnKnow %s %s\n",TRANSFER_ENCODING,value);
            }
        }
        if (pos2 == posend){
            break;
        }
        pos1 = pos2 + HTTPFLAGLEN;//定位在\r\n之后
    }
    
    if(statusCode >= 100 && statusCode <= 101){
        
    }else if(statusCode >= 200 && statusCode <= 206){
        
    }else if(statusCode >= 300 && statusCode <= 305){
        
    }else if(statusCode >= 400 && statusCode <= 415){
        
    }else if(statusCode >= 500 && statusCode <= 505){
        
    }else{
        statusCode = -1;
    }

    _checkhttp = false;
    return statusCode;
}


bool HttpRequest::SendRequest(char * param)
{
    char content[1024];
    content[0] = 0;
    strcat(content, "GET ");
    strcat(content, _url.c_str());
    strcat(content, " HTTP/1.1\r\n");
    strcat(content, "Host: ");
    strcat(content, _hostname.c_str());
    strcat(content, "\r\n");
    strcat(content, "Accept: */*\r\n");
    strcat(content, "User-Agent: hifun-1.0.0\r\n");
    
    char * pCur = param;
    if(pCur){
        char item[100];
        char * pPos = pCur;
        while(pPos != NULL){
            pPos = strchr(pCur, ' ');
            if(pPos != NULL){
                memcpy(item, pCur, pPos - pCur);
                item[pPos - pCur] = 0x00;
                if(pPos != NULL){
                    pCur = pPos + 1;
                }
            }else{
                strcpy(item, pCur);
            }

            strcat(content, item);
            strcat(content, "\r\n");
        }
    }
    
    strcat(content, "\r\n");

    int length = (int)strlen(content);
    int sent = _socket->Send(content, length);
    return sent == ErrCodeNone;
}

void HttpRequest::CompleteRequest(int sockErr)
{
    if(_socket){
        AsyncSocket::Release(_socket);
        _socket = NULL;
    }
    
    _status = HttpClosed;
}

int  HttpRequest::GetChunk(const char*buff,int len,int*chunksize,int*countchunksize)
{
    const char*pos = buff;
    const char*pos2= NULL;
    int tempchunksize = *chunksize;
    int tempcountchunksize = *countchunksize;
    int chunkbufpos = 0;
    while((pos-buff) < len){
        if (tempchunksize == 0){
            sscanf(pos, " %x", &tempchunksize);
            pos2 = strstr(pos,HTTPFLAG);
            pos  = pos2+HTTPFLAGLEN;
            if (tempchunksize == 0){
                printf("Get End\n");
                return tempcountchunksize;
            }
        }
        int needsize = tempchunksize-tempcountchunksize;//需要的字节数
        int cangetsize = len-(int)(pos-buff);//能给的字节数
        int realsize = needsize<cangetsize?needsize:cangetsize;
        memcpy(_chunckBuffer + chunkbufpos,pos,realsize);
        chunkbufpos += realsize;
        tempcountchunksize+=realsize;
        pos+=realsize;
        if (tempcountchunksize==tempchunksize){
            tempchunksize=0;
            tempcountchunksize=0;
        }
    }
    *chunksize=tempchunksize;
    *countchunksize=tempcountchunksize;
    return tempcountchunksize;
}


/// ISocketDelegate
void HttpRequest::HandleSocketEvent(AsyncSocket *socket, Event sockEvent)
{
    
}


