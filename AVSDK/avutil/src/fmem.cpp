
#include "../include/fmem.h"
#include "../include/logging.h"
using namespace MediaCloud::Common;

#ifdef WIN32
#include <Windows.h>
static DWORD _fmemTlsIdx = TLS_OUT_OF_INDEXES;
#else
#include <pthread.h>
static pthread_key_t _fmemPThreadKey = 0;
#endif

#ifdef WIN32
void InitializeFastMem() {
    _fmemTlsIdx = TlsAlloc();
}
#else
namespace MediaCloud { namespace Common {
    void InitializeFastMem() {
        pthread_key_create(&_fmemPThreadKey, nullptr);
    }
}}
#endif

#define APPEND_TO_LIST(ihdr, listHdr) do { \
    (ihdr)->next = &(listHdr); \
    (ihdr)->prev = (listHdr).prev; \
    (listHdr).prev->next = (ihdr); \
    (listHdr).prev = (ihdr); \
} while (0) \

#define REMOVE_FROM_LIST(ihdr) do { \
    (ihdr)->prev->next = (ihdr)->next; \
    (ihdr)->next->prev = (ihdr)->prev; \
} while (0) \

FastThreadBuffer* FastThreadBuffer::GetCurrent() {
    FastThreadBuffer *ftbuf = nullptr;
#ifdef WIN32
    ftbuf = reinterpret_cast<FastThreadBuffer*>(TlsGetValue(_fmemTlsIdx));
#else
    ftbuf = reinterpret_cast<FastThreadBuffer*>(pthread_getspecific(_fmemPThreadKey));
#endif
    if (ftbuf == nullptr) {
        ftbuf = new FastThreadBuffer();
#ifdef WIN32
        TlsSetValue(_fmemTlsIdx, ftbuf);
#else
        pthread_setspecific(_fmemPThreadKey, ftbuf);
#endif
    }
    return ftbuf;
}

void FastThreadBuffer::FreeCurrent() {
    FastThreadBuffer *ftbuf = nullptr;
#ifdef WIN32
    ftbuf = reinterpret_cast<FastThreadBuffer*>(TlsGetValue(_fmemTlsIdx));
#else
    ftbuf = reinterpret_cast<FastThreadBuffer*>(pthread_getspecific(_fmemPThreadKey));
#endif
    if (ftbuf != nullptr) {
#ifdef WIN32
        TlsSetValue(_fmemTlsIdx, nullptr);
#else
        pthread_setspecific(_fmemPThreadKey, nullptr);
#endif
        delete ftbuf;
    }
}

#define CACHE_PREFIX_MARK 0x12345678
#define CACHE_SUFFIX_MARK 0xAABBAABB

FastThreadBuffer::FastThreadBuffer() 
    : _caches (20, sizeof(Cache*)) {
}

FastThreadBuffer::~FastThreadBuffer() {
    FastCircleQueue::SlotPtr sptr = _caches.FirstSlot();
    int slotnum = _caches.NumOfSlots();
    while (slotnum-- > 0) {
        Cache *cache = *reinterpret_cast<Cache**>(sptr);
        if (cache != nullptr) {
            DestoryCache(cache);
        }
        sptr = _caches.NextSlot(sptr);
    }
}

void FastThreadBuffer::AllocateBufferInCache(Cache *cache) {
    if (cache->bufferList.Count() > 0) {
        LogWarning("fmem", "alloc more buffer itemsize %d, bufcnt %d, cap %d, busy %d, free %d\n", 
            cache->itemSize, cache->bufferList.Count(), cache->capability, cache->busyCnt, cache->freeCnt);
    }

    FastList::Node *newNode = cache->bufferList.Append();
    ItemHdr *pitem = reinterpret_cast<ItemHdr*>(FastList::NodeToBody(newNode));
    for (int i = 0; i < cache->capability; i++) {
        APPEND_TO_LIST(pitem, cache->freeHdr);
        cache->freeCnt++;
        pitem = (ItemHdr*)((char*)(pitem + 1) + cache->itemSize + 16);
    }
}

void FastThreadBuffer::DestoryCache(Cache *cache) {
    AssertMsg(cache->busyCnt == 0, "some items were not freed");
    AssertMsg(cache->freeCnt == cache->capability * cache->bufferList.Count(), "free cnt invalid");
    cache->bufferList.Reset(nullptr);
    delete cache;
}

inline FastThreadBuffer::Cache* FastThreadBuffer::GetCacheByIdx(
    int cacheIdx, bool setNull) {
    AssertMsg(cacheIdx >= 0 && cacheIdx < _caches.NumOfSlots(), "invalid cacheIdx");
    FastCircleQueue::SlotPtr pslot = _caches.At(cacheIdx);
    Cache *cache = *reinterpret_cast<Cache**>(pslot);
    AssertMsg(cache != nullptr, "re-unregister cache");
    if (setNull) {
        *reinterpret_cast<Cache**>(pslot) = nullptr;
    }
    return cache;
}

/*
* Before allocating memory, the allocation size must be registered first
* with the capability
*/
int FastThreadBuffer::RegisterCache(int itemSize, int capability) {
    // we don't check the dup itemSize, it is the responsiblity for client
    AssertMsg(itemSize > 0 && capability > 0, "invalid item size or capability");
    if (itemSize < 8) {
        itemSize = 8;
    }
    else {
        // align the itemSize with 8
        itemSize = (itemSize + 7) & (~7);
    }

    if (capability < 10) {
        capability = 10;
    }

    Cache *cache = new Cache((itemSize + sizeof(ItemHdr) + 16) * capability);
    cache->itemSize = itemSize;
    cache->capability = capability;
    cache->busyCnt = cache->freeCnt = 0;
    cache->freeHdr.prev = cache->freeHdr.next = &cache->freeHdr;
    cache->busyHdr.prev = cache->busyHdr.next = &cache->busyHdr;
    AllocateBufferInCache(cache);

    FastCircleQueue::SlotPtr pslot = _caches.AppendSlot();
    *reinterpret_cast<Cache**>(pslot) = cache;
    return _caches.NumOfSlots() - 1;
}

/*
* All allocated from this cache must be freed firstly
*/
void FastThreadBuffer::UnregisterCache(int cacheIdx) {
    Cache *cache = GetCacheByIdx(cacheIdx, true);
    DestoryCache(cache);
}

void* FastThreadBuffer::AllocFromCache(int cacheIdx) {
    Cache *cache = GetCacheByIdx(cacheIdx, false);
    if (cache->freeCnt == 0) {
        AllocateBufferInCache(cache);
    }
    // remove from free list
    ItemHdr *itemHdr = cache->freeHdr.next;
    REMOVE_FROM_LIST(itemHdr);
    APPEND_TO_LIST(itemHdr, cache->busyHdr);
    cache->freeCnt--;
    cache->busyCnt++;
    int *pPrefix = (int*)(itemHdr + 1);
    pPrefix[0] = CACHE_PREFIX_MARK;
    pPrefix[1] = CACHE_PREFIX_MARK;
    int *pSuffix = (int*)((char*)pPrefix + 8 + cache->itemSize);
    pSuffix[0] = CACHE_SUFFIX_MARK;
    pSuffix[1] = CACHE_SUFFIX_MARK;
    return (char*)pPrefix + 8;
}

void FastThreadBuffer::FreeToCache(int cacheIdx, void *buffer) {
    if (buffer == nullptr) {
        return;
    }

    Cache *cache = GetCacheByIdx(cacheIdx, false);

    int *pPrefix = (int*)((char*)buffer - 8);
    int *pSuffix = (int*)((char*)buffer + cache->itemSize);
    AssertMsg(pPrefix[0] == CACHE_PREFIX_MARK && pPrefix[1] == CACHE_PREFIX_MARK, "prefix mark error");
    AssertMsg(pSuffix[0] == CACHE_SUFFIX_MARK && pSuffix[1] == CACHE_SUFFIX_MARK, "suffix mark error");
    
    pPrefix[0] = pPrefix[1] = pSuffix[0] = pSuffix[1] = 0;

    ItemHdr *itemHdr = reinterpret_cast<ItemHdr*>(pPrefix) - 1;
    REMOVE_FROM_LIST(itemHdr);
    APPEND_TO_LIST(itemHdr, cache->freeHdr);
    cache->freeCnt++;
    cache->busyCnt--;
}
