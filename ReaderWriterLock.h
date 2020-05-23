//
// Created by mguzek on 15.05.2020.
//

#ifndef SCALABLE_FAIR_READER_WRITER_LOCK_READERWRITERLOCK_H
#define SCALABLE_FAIR_READER_WRITER_LOCK_READERWRITERLOCK_H

#include <atomic>
#include <threads.h>
#include <cstdint>
#include "SNZI.h"

class ReaderWriterLock {
public:
    struct Node {
        enum class LockType { READER, WRITER };
        std::atomic<LockType> type;
        std::atomic<bool> locked;
        std::atomic<Node*> next;
        bool waitForNext;               //field only for readers

        Node() : waitForNext{false} {
            next.store(nullptr);
        }
    };

    class ReaderLock {
        ReaderWriterLock* const enclosingObject;

    public:
        explicit ReaderLock(ReaderWriterLock* owner) : enclosingObject{owner} {

        }

        void lock(const unsigned threadID, Node* myNode) {
            //enclosingObject->readers_count.fetch_add(1, std::memory_order_seq_cst);
            enclosingObject->snzi.arrive(threadID);
            if(enclosingObject->tail.load(std::memory_order_seq_cst) == nullptr) {
                return;
            }

            myNode->type.store(Node::LockType::READER, std::memory_order_release);

            Node* pred = enclosingObject->tail.exchange(myNode, std::memory_order_seq_cst);
            Node* copyForCAS = myNode;
            if(pred == nullptr && enclosingObject->tail.compare_exchange_strong(copyForCAS, nullptr, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                return;
            }

            if(pred != nullptr) {
                //enclosingObject->readers_count.fetch_sub(1, std::memory_order_seq_cst);
                enclosingObject->snzi.depart(threadID);
                myNode->locked.store(true, std::memory_order_release);
                pred->next.store(myNode, std::memory_order_release);
                while(myNode->locked.load(std::memory_order_acquire)) { //sort of unfortunate chain of loops for chain of READERS
                    thrd_yield();
                }
                //enclosingObject->readers_count.fetch_add(1, std::memory_order_seq_cst);
                enclosingObject->snzi.arrive(threadID);
            }

            if(myNode->next.load(std::memory_order_acquire) == nullptr) {
                copyForCAS = myNode;
                if(enclosingObject->tail.load(std::memory_order_seq_cst) == nullptr
                   || enclosingObject->tail.compare_exchange_strong(copyForCAS, nullptr, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    return;
                }
                while(myNode->next.load(std::memory_order_acquire) == nullptr) {
                    thrd_yield();
                }
            }

            myNode->waitForNext = true;
            if(myNode->next.load(std::memory_order_acquire)->type.load(std::memory_order_acquire) == Node::LockType::READER) {
                myNode->next.load(std::memory_order_acquire)->locked.store(false, std::memory_order_release);
                myNode->waitForNext = false;
            }
        }

        void unlock(const unsigned threadID, Node* myNode) {
            //enclosingObject->readers_count.fetch_sub(1, std::memory_order_seq_cst);
            enclosingObject->snzi.depart(threadID);

            if(myNode->waitForNext) {
                while(myNode->next.load(std::memory_order_acquire) == nullptr) {
                    thrd_yield();
                }
                myNode->next.load(std::memory_order_acquire)->locked.store(false, std::memory_order_release);
            }
            myNode->next.store(nullptr, std::memory_order_release);
            myNode->waitForNext = false;
        }
    };

    class WriterLock {
        ReaderWriterLock* const enclosingObject;

    public:
        explicit WriterLock(ReaderWriterLock* owner) : enclosingObject{owner} {

        }

        void lock(const unsigned threadID, Node* myNode) {
            myNode->type.store(Node::LockType::WRITER, std::memory_order_release);

            Node* pred = enclosingObject->tail.exchange(myNode, std::memory_order_seq_cst);

            if(pred != nullptr) {
                myNode->locked.store(true, std::memory_order_release);
                pred->next.store(myNode, std::memory_order_release);
                while(myNode->locked.load(std::memory_order_acquire)) {
                    thrd_yield();
                }
            }

            //while(enclosingObject->readers_count.load(std::memory_order_seq_cst) > 0) {
            while(enclosingObject->snzi.query()) {
                thrd_yield();
            }
        }

        void unlock(const unsigned threadID, Node* myNode) {
            if(myNode->next.load(std::memory_order_acquire) == nullptr) {
                Node* copyForCAS = myNode;
                if(enclosingObject->tail.compare_exchange_strong(copyForCAS, nullptr, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    return;
                }

                while(myNode->next.load(std::memory_order_acquire) == nullptr) {
                    thrd_yield();
                }
            }

            myNode->next.load(std::memory_order_acquire)->locked.store(false, std::memory_order_release);
            myNode->next.store(nullptr, std::memory_order_release);
        }
    };

private:
    std::atomic<Node*> tail;
    //std::atomic<uint64_t> readers_count;
    SNZI snzi;
    ReaderLock reader_Lock;
    WriterLock writer_Lock;

public:
    ReaderWriterLock(unsigned depthSNZI) : snzi{depthSNZI}, reader_Lock{this}, writer_Lock{this} {
        tail.store(nullptr, std::memory_order_release);
        //readers_count.store(0, std::memory_order_release);
    }

    ReaderLock& readerLock() {
        return reader_Lock;
    }

    WriterLock& writerLock() {
        return writer_Lock;
    }
};

#endif //SCALABLE_FAIR_READER_WRITER_LOCK_READERWRITERLOCK_H