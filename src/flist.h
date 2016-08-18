
#ifndef _CPPCMN_FLIST_H
#define _CPPCMN_FLIST_H

#include "logging.h"

using namespace MediaCloud::Common;

namespace cppcmn {

    struct ListHead {
        ListHead *next, *prev;
    };

    inline void ListInitHead(ListHead *hdr) {
        hdr->next = hdr->prev = hdr;
    }

    inline bool ListIsEmpty(const ListHead *hdr) {
        bool empty = hdr->next == hdr;
        if (empty) {
            Assert(hdr->prev == hdr);
        }
        return empty;
    }

    inline void ListInsert(ListHead *prev, ListHead *next, ListHead *newItem) {
        newItem->next = next;
        newItem->prev = prev;
        prev->next = newItem;
        next->prev = newItem;
    }

    inline void ListInsertAfter(ListHead *pos, ListHead *newItem) {
        ListInsert(pos, pos->next, newItem);
    }

    // !! If pos is the header, the newItem will be added at tail !!
    inline void ListInsertBefore(ListHead *pos, ListHead *newItem) {
        ListInsert(pos->prev, pos, newItem);
    }

    inline void ListAddToEnd(ListHead *hdr, ListHead *item) {
        ListInsert(hdr->prev, hdr, item);
    }

    inline void ListAddToBegin(ListHead *hdr, ListHead *item) {
        ListInsert(hdr, hdr->next, item);
    }

    inline void ListRemove(ListHead *item) {
        item->next->prev = item->prev;
        item->prev->next = item->next;
        item->next = item->prev = nullptr;
    }

    inline void ListMove(ListHead *newhdr, ListHead *item) {
        ListRemove(item);
        ListAddToEnd(newhdr, item);
    }

    inline void ListReplace(ListHead *newItem, ListHead *oldItem) {
        newItem->next = oldItem->next;
        newItem->next->prev = newItem;
        newItem->prev = oldItem->prev;
        newItem->prev->next = newItem;
        oldItem->next = oldItem->prev = nullptr;
    }

    inline void ListReplaceHeader(ListHead *hdr, ListHead *newhdr) {
        if (ListIsEmpty(hdr)) {
            ListInitHead(newhdr);
        }
        else {
            newhdr->next = hdr->next;
            newhdr->prev = hdr->prev;
            hdr->next->prev = newhdr;
            hdr->prev->next = newhdr;
            ListInitHead(hdr);
        }
    }

    inline bool ListIsLast(ListHead *hdr, ListHead *item) {
        bool last = item->next == hdr;
        if (last) {
            Assert(hdr->prev == item);
        }
        return last;
    }

    inline bool ListIsFirst(ListHead *hdr, ListHead *item) {
        bool first = item->prev == hdr;
        if (first) {
            Assert(hdr->next == item);
        }
        return first;
    }

    /*
        Search from tail for inserting
        returning non-null -> add new item after it, it maybe the header; or a dup item
        never null

        comparer - 0 : equal; -1 - move forward; 1 - move backward
    */
    typedef int (*ListInsertComparer) (ListHead *item, void *tag);
    inline ListHead* ListInsertFromTail(ListHead *hdr, ListInsertComparer comparer, void *tag, bool &isdup) {
        isdup = false;
        ListHead *iter = hdr->prev;
        while (iter != hdr) {
            int ret = comparer(iter, tag);
            if (ret >= 0) {
                isdup = ret == 0;
                break;
            }
            iter = iter->prev;
        }
        return iter;
	}

#define LIST_LOOP_BEGIN(hdr, T) do { \
	ListHead *liter = (hdr).next; \
	while (liter != &(hdr)) { \
		T *litem = static_cast<T*>(liter); \
		liter = liter->next;		\

#define LIST_LOOP_END }}while(0)

}

#endif
