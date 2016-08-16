
#ifndef _MEDIACLOUD_HTTP_H
#define _MEDIACLOUD_HTTP_H

#include <map>
#include <string>
#include "msgqueue.h"
#include "socket.h"


namespace MediaCloud
{
    namespace Common
    {
#define HTTPSTREAMRECVLEN 100000
        class HttpRequest : public ISocketDelegate
        {
        public:
            typedef struct _ResponseInfo{
                unsigned int content_length;
                unsigned int content_fileSize;
                unsigned int rangeStart;
                unsigned int rangeEnd;
            }ResponseInfo;
            
            virtual ~HttpRequest();

            enum Method
            {
                MethodGet,
            /// MethodPost,
            };

            static HttpRequest* Create(const char *url, Method method, unsigned int timeout = 20 * 000);
            
            int Connect(bool *socket_running_flag);
            int Begin(Method httpMethod, char * param = NULL);
            int ReadResponseData(char ** buffer, int & len, ResponseInfo & resInfo);
            int ReadData(char **buffer, ResponseInfo & resInfo);
            int End();
            
            bool GetIP(char *& localIP, char *& remoteIP);

        protected:

            /// ISocketDelegate
            virtual void HandleSocketEvent(AsyncSocket *socket, Event sockEvent);

        private:
            HttpRequest(const char *url, const char *hostname, Method method,  
                     unsigned int timeout);


            void CompleteRequest(int sockErr);
            bool SendRequest(char * param = NULL);
            int ParseHeader(char buf[], int bufLeng,const char**datastartpos, char *& nParam, unsigned int & nParamLen, unsigned int & rangeStart, unsigned int & rangeEnd);
            int  GetChunk(const char*buff,int len,int*chunksize,int*countchunksize);
            enum
            {
                HttpMsgTimeout = 21
            };

            enum HttpStatus
            {
                HttpNone,
                HttpConnecting,
                HttpRecving,
                HttpClosed,
            } _status;

            std::string _url;
            std::string _hostname;
            Method _method;
            unsigned int _timeout;
            AsyncSocket *_socket;
            bool _checkhttp;
            char _recvBuffer[HTTPSTREAMRECVLEN];
            char _chunckBuffer[HTTPSTREAMRECVLEN];
            int _fileSize;//远端文件总长度
            int _receiveFilelength;//本次请求文件总长
            int _chuncksize;//分片大小
            int _countchunksize;//累计分片字节数
            int _usechunk;//是否使用分片
        };
    }
}

#endif
