
#ifndef _MEDIACLOUD_FLIST_H
#define _MEDIACLOUD_FLIST_H


namespace MediaCloud { namespace Common {

    class FastList {
    public:
        struct Node {
            Node* next;
            Node* prev;
        };
        __inline static void* NodeToBody(Node *node) {
            return node + 1;
        }
        __inline static Node* BodyToNode(void *body) {
            return reinterpret_cast<Node*>(body) - 1;
        }

        //wlj enum class VisitReason {
        enum VisitReason {
            Release,
        };
        typedef void(*BodyVisitor) (Node *node, VisitReason reason);

        FastList(int bodySize)
            : _bodySize(bodySize)
            , _cnt(0) {
            _hdr.next = &_hdr;
            _hdr.prev = &_hdr;
        }

        ~FastList() {
            Reset(nullptr);
        }

        // clear all nodes, and call visitor to destory body
        void Reset(BodyVisitor visitor) {
            Node *ptr = _hdr.next;
            while (ptr != &_hdr) {
                Node *todel = ptr;
                ptr = ptr->next;
                if (visitor != nullptr) {
                    visitor(todel, Release);
                }
                free(todel);
            }
            _hdr.next = &_hdr;
            _hdr.prev = &_hdr;
            _cnt = 0;
        }

        __inline int Count() const { return _cnt; }
        __inline bool Empty() const { return _cnt == 0; }

        __inline Node* Append() {
            Node *newNode = (Node*)malloc(sizeof(Node) + _bodySize);
            newNode->next = &_hdr;
            newNode->prev = _hdr.prev;
            _hdr.prev->next = newNode;
            _hdr.prev = newNode;
            ++_cnt;
            return newNode;
        }

        // unlink one other's node and append to this one
        __inline void Append(FastList &other, Node *nodeInOther) {
            AssertMsg(this != &other && _bodySize == other._bodySize,
                "invalid append other's node");
            // remove from other list
            nodeInOther->prev->next = nodeInOther->next;
            nodeInOther->next->prev = nodeInOther->prev;
            other._cnt--;
            // insert to tail
            nodeInOther->next = &_hdr;
            nodeInOther->prev = _hdr.prev;
            _hdr.prev->next = nodeInOther;
            _hdr.prev = nodeInOther;
            ++_cnt;
        }

        // append from all nodes of others, and the other will be clear
        __inline void Append(FastList &other) {
            AssertMsg(this != &other && _bodySize == other._bodySize, 
                "invalid append list");
            if (other._cnt == 0) {
                return; // nothing to move
            }

            // move in the whole chain
            other._hdr.next->prev = _hdr.prev;
            other._hdr.prev->next = &_hdr;
            _hdr.prev->next = other._hdr.next;
            _hdr.prev = other._hdr.prev;

            _cnt += other._cnt;

            // clean the other
            other._hdr.prev = &other._hdr;
            other._hdr.next = &other._hdr;
            other._cnt = 0;
        }

        __inline Node* InsertBefore(Node *pos) {
            Node *newNode = (Node*)malloc(sizeof(Node) + _bodySize);
            newNode->next = pos;
            newNode->prev = pos->prev;
            pos->prev->next = newNode;
            pos->prev = newNode;
            ++_cnt;
            return newNode;
        }

        __inline Node* InsertAfter(Node *pos) {
            Node *newNode = (Node*)malloc(sizeof(Node) + _bodySize);
            newNode->next = pos->next;
            newNode->prev = pos;
            pos->next->prev = newNode;
            pos->next = newNode;
            ++_cnt;
            return newNode;
        }

        // comparer - 0 : equal; -1 - move forward; 1 - move backward
        __inline Node* InsertBySearch(
            bool beginFromTail, int(*comparer)(Node*, void*), void *compEle, bool &dup) {
            dup = false;
            if (_cnt == 0) {
                return Append();
            }
            
            Node* ptr = beginFromTail ? _hdr.prev : _hdr.next;
            while (ptr != &_hdr) {
                int cret = comparer(ptr, compEle);
                if (cret == 0) {
                    dup = true;
                    return ptr;
                }

                if ((cret < 0 && beginFromTail) || (cret > 0 && !beginFromTail)) {
                    ptr = beginFromTail ? ptr->prev : ptr->next;
                    continue;
                }

                // okay, find place to insert
                return beginFromTail ? InsertAfter(ptr) : InsertBefore(ptr);
            }

            return beginFromTail ? InsertBefore(_hdr.next) : Append();
        }

        // delete the current one, and return the next one if there is
        __inline Node* Delete(Node *pos) {
            Node *next = NextNode(pos);
            pos->prev->next = pos->next;
            pos->next->prev = pos->prev;
            free(pos);
            --_cnt;
            return next;
        }

        __inline Node* FirstNode() const {
            return _cnt == 0 ? nullptr : _hdr.next;
        }

        __inline Node* LastNode() const {
            return _cnt == 0 ? nullptr : _hdr.prev;
        }

        __inline Node* PrevNode(Node *pos) const {
            pos = pos->prev;
            return pos == &_hdr ? nullptr : pos;
        }

        __inline Node* NextNode(Node *pos) const {
            pos = pos->next;
            return pos == &_hdr ? nullptr : pos;
        }

    private:
        int _cnt;
        int _bodySize;
        Node _hdr;
    };
}}

#endif
