#ifndef _CPPCMN_SOCKET_H
#define _CPPCMN_SOCKET_H

#include <stdint.h>
#include <stdio.h>
#include "flist.h"
#include "misc.h"

namespace cppcmn {

    // Representing an IPv4 socket address
    struct SocketAddr {
        SocketAddr() {
            *(uint32_t*)ipv4 = 0;
			port = 0;
        }

        /*
        *   addr is null-terminated
        */
        static bool Parse(const char *addr, SocketAddr &socketAddr);

        inline bool IsIPEqual(const SocketAddr &other) const {
            return *(uint32_t*)ipv4 == *(uint32_t*)other.ipv4;
        }

        inline bool IsEqual(const SocketAddr &other) const {
            return IsIPEqual(other) && port == other.port;
        }

		inline int ToString(char buf[]) const {
			return sprintf(buf, "%u.%u.%u.%u:%u", ipv4[0], ipv4[1], ipv4[2], ipv4[3], port);
		}

        uint8_t ipv4[4];
        int port;
    };
    
#define SOCKETADDR_TO_TMPSTR(sa) char __tmpsaddr[32]; (sa).ToString(__tmpsaddr)
#define SOCKETADDR_TO_TMPSTR2(sa) char __tmpsaddr2[32]; (sa).ToString(__tmpsaddr2)
    
    /*
     * SO_REUSEPORT (since Linux 3.9) allow multiple processes handling on same UDP port !
     */
    class SocketPoll;
    class AsyncSocket {
    public:
        typedef int Handle;
        static const Handle InvalidHandle = -1;

        enum {
            ErrNone = 0,
            ErrUnknown = -1,
            ErrBlocked = -2,
            ErrClosed = -3,
        };

        struct Tag {
            void *ptrVal;
            int intVal;
            
            Tag() : ptrVal(nullptr), intVal(0) {}
            Tag(void *p, int i) : ptrVal(p), intVal(i) {}
        };

        static inline bool IsValidHandle(Handle handle) {
            return handle != InvalidHandle;
        }
        
        // a quick method to close the just accepted handle
        static void CloseHandle(Handle handle);

        AsyncSocket();
        ~AsyncSocket();

        int  CreateTcp(const Tag *tag);
        int  CreateUdp(const Tag *tag);
        void CreateFromHandle(Handle handle, bool isUdp, const Tag *tag);
        // take all things from other, the other will be set to invalid
        // it is just working as a deep copy
        void CreateFromOther(AsyncSocket &other);

        // close or clean this socket, and the socket must not be in a poll
        void Reset(bool justClean = false);

        // During Bind/Listen/Connect, AsyncSocket must not be in poll !
        // addr returns the read binded address
        int Bind(SocketAddr &addr);
        int Listen(int backlog);
        // returning ErrNone means the connection succeeded syncly
        int Connect(const SocketAddr &rmtAddr);

        // IMPORTANT: checking socket == null even Accept returning ErrNone
        int Accept(Handle &handle, SocketAddr &rmtAddr);

        /*
        *   If the socket is not in poll, it is a simple recv/recvfrom on socket
        *   If the socket is in poll, it must be in ReadyList of the SocketPoll,
        *   Clear the EPOLLIN flag in SocketPoll's ReadyList,
        *   if recv/recvfrom returns EAGAIN, less cnt than bufLength, other error
        *
        *   If the socket just have same bytes as bufLength, the EPOLLIN flag will
        *   not be cleared. If the client stop reading, the socket will stay in ReadyList,
        *   The next call to EPoll returns it again.
        */
        int Recv(void *buffer, int bufLength);
        int RecvFrom(void *buffer, int bufLength, SocketAddr &rmtAddr);

        int Send(const void *buffer, int bufLength, int flag = 0);
        int SendTo(const void *buffer, int bufLength, const SocketAddr &rmtAddr);

        int SetSendBufferSize(int bufsize);
        int SetRecvBufferSize(int bufsize);
        int GetSendBufferSize() const { return _sndBufferSize; }
        int GetRecvBufferSize() const { return _recvBufferSize; }
        int GetUnsentBytes(); // ioctl SIOCOUTQ
        int SetNoDelay(bool noDelay);
        // idleTimeout - timeout for sending probe after no data packet in seconds
        // interval - probe interval in seconds
        // probes - socket error when no probe ack for probes times
        int SetKeepAlive(bool enable, int idleTimeout, int interval, int probes);

        // when client get ReadyWritable event on a sendblocked socket
        // at least, the client should call this to reset the blocked flag on the socket
        void SetSendUnblocked();
        inline bool IsSendBlocked() const { return _sendBlocked; }
        
        // If this socket is set in a poll, this is the index of poll items
        // Otherwise, -1
        inline int PollIndex() const { return _pollIdx; }
        inline bool InPoll() const { return _pollIdx >= 0; }

        inline bool IsValid() const { return _handle != InvalidHandle; }
        inline Handle GetHandle() const { return _handle; }
        inline const Tag& GetTag() const { return _tag; }

        /*
        *   If the socket is in poll, we need set the tag to poll also
        */
        void SetTag(const Tag &tag);

    private:
        DISALLOW_COPY_AND_ASSIGN(AsyncSocket);
        
        int CreateCommon(Handle s, bool udp, const Tag *tag);
        static int GetBufferSizeInternal(Handle s, bool sndbuffer);

        Handle _handle;
        int _pollIdx;
        SocketPoll *_poll;
        Tag _tag;
        bool _isUdp;
        bool _sendBlocked;
        int _sndBufferSize;
        int _recvBufferSize;

        friend class SocketPoll;
    };

    class SocketPoll {
    public:

        /*
         *  Client should handle Readable eariler than Error,
         *  Because a peer may send some data, and close the socket right after that
         *  If handling Error first, the final protocol from peer may be missed.
         */
        enum ReadyEvents {
            // Must be same with <sys/epoll.h>
            ReadyReadable = 1,
            ReadyWritable = 4,
            ReadyError = 8,
        };

        enum class AddPurpose {
            Accept = 1 << 8,
            Data = 1 << 9
        };

        /*
        *   A poll has a fixed capability of poll items
        */
        static SocketPoll* Create(int capability);
        ~SocketPoll();

        void Add(AsyncSocket &asock, AddPurpose purpose);
        void Remove(AsyncSocket &asock);

        /*
        *   The poll maintains 3 list for the in-poll sockets
        *   At any time, a socket must only be in one of these 3 lists
        *   Free list: those slots are free for adding more sockets in the poll
        *   Poll list: those sockets are under epoll monitoring
        *   Ready list: those sockets are waiting for client to handle
        *
        *   During handling the socket in ReadyItems that the PollReady returned
        *   Some sockets may be destoryed or created into the same slot in the ReadyItems
        *   So IsStillReady must be called to check if a ReadyItem is still valid,
        *   Because any new socket created after PollReady must not be in Ready list.
        */
        struct ReadyItem {
            AsyncSocket::Handle handle;
            AsyncSocket::Tag tag;
            int pollIdx;
            int evts;
        };
        int PollReady(ReadyItem readyItems[], int readyNum, int timeout);

        /*
        *   Just check if the item is still in the ready list
        *   If true, the returned socket handle must be still valid
        *   Its handle and tag can be used safely to find the client data associated with this item
        */
        bool IsStillReady(const ReadyItem &item) const;

        /*
        *	How many sockets registered in this poll
        */
        int NumOfSockets() const { return _socketNum; }

    private:
        struct Item : ListHead {
            ListHead *hdr;
            int idx;
            int pollEvts;
            int readyEvts;
            AsyncSocket::Handle handle;
            AsyncSocket::Tag tag;
            bool sendBlocked;
        };

        SocketPoll(int capability, int epoll, Item *pitems, void *pEvents);
        SocketPoll(const SocketPoll &other) = delete;
        SocketPoll& operator=(const SocketPoll &other) = delete;
        void ResetSocketTag(AsyncSocket *asock);
        void ResetSocketSendBlocked(AsyncSocket *asock);
        void ClearPollReadyReadableEvt(AsyncSocket *asock);

        Item *_items;
        ListHead _freeHeader;
        ListHead _pollHeader;
        ListHead _readyHeader;
        int _socketNum;
        int _capability;
        int _epoll;
        void *_pollEvents;

        friend class AsyncSocket;
    };
    
    /*
     *  A helper function to create a pair of 2 sockets
     *  Then they can send data to each other as a local pipe
     */
    bool CreateAsyncSocketPair(AsyncSocket &sndSocket, AsyncSocket &recvSocket);
}


#endif
