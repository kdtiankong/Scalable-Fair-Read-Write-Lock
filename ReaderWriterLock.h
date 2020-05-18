//
// Created by mguzek on 15.05.2020.
//

#ifndef SCALABLE_FAIR_READER_WRITER_LOCK_READERWRITERLOCK_H
#define SCALABLE_FAIR_READER_WRITER_LOCK_READERWRITERLOCK_H

#include <atomic>
#include <threads.h>
#include <unordered_map>
#include <cstdint>

class ReaderWriterLock {
    struct Node {
        enum LockType { READER, WRITER };
        std::atomic<LockType> type;
        std::atomic<bool> locked;
        std::atomic<Node*> next;
        bool waitForNext;               //field only for readers

        Node() : waitForNext{false} {
            next.store(nullptr);
        }
    };

public:
    class MyUniquePtr {
        Node* ptr;
    public:
        MyUniquePtr() { ptr = nullptr; }
        explicit MyUniquePtr(Node* addr) : ptr{addr} { }
        MyUniquePtr& operator=(MyUniquePtr&& other) noexcept { ptr = std::exchange(other.ptr, nullptr); return *this; }
        Node* get() const { return ptr; }
        ~MyUniquePtr() { delete ptr; }
    };

    class ReaderLock {
        ReaderWriterLock* const enclosingObject;

    public:
        explicit ReaderLock(ReaderWriterLock* owner) : enclosingObject{owner} {

        }

        void lock() {
            enclosingObject->readers_count.fetch_add(1);
            if(myNode.count(enclosingObject) == 0) {
                myNode[enclosingObject] = std::move(MyUniquePtr{new Node{}});
            }
            if(enclosingObject->tail.load() == nullptr) {
                return;
            }

            Node* node = myNode[enclosingObject].get();
            node->type.store(Node::LockType::READER);

            Node* pred = enclosingObject->tail.exchange(node);
            if(pred == nullptr) {
                return;
            }
            enclosingObject->readers_count.fetch_sub(1);

            node->locked.store(true);
            pred->next.store(node);
            while(node->locked.load()) { //sort of unfortunate chain of loops for chain of READERS
                thrd_yield();
            }
            enclosingObject->readers_count.fetch_add(1);

            node->waitForNext = true;
            if(node->next.load() == nullptr) {
                if(enclosingObject->tail.compare_exchange_strong(node, nullptr)) {
                    node->waitForNext = false;
                    return;
                }
                else {
                    while(node->next.load() == nullptr) {
                        thrd_yield();
                    }
                }
            }

            if(node->next.load()->type.load() == Node::LockType::READER) {
                node->next.load()->locked.store(false);
                node->waitForNext = false;
            }
        }

        void unlock() {
            enclosingObject->readers_count.fetch_sub(1);
            Node* node = myNode[enclosingObject].get();

            if(node->waitForNext) {
                while(node->next.load() == nullptr) {
                    thrd_yield();
                }
                node->next.load()->locked.store(false);
            }

            node->next.store(nullptr);
        }
    };

    class WriterLock {
        ReaderWriterLock* const enclosingObject;

    public:
        explicit WriterLock(ReaderWriterLock* owner) : enclosingObject{owner} {

        }

        void lock() {
            if(myNode.count(enclosingObject) == 0) {
                myNode[enclosingObject] = std::move(MyUniquePtr{new Node{}});
            }
            Node* node = myNode[enclosingObject].get();
            node->type.store(Node::LockType::WRITER);

            Node* pred = enclosingObject->tail.exchange(node);
            if(pred != nullptr) {
                node->locked.store(true);
                pred->next.store(node);
                while(node->locked.load()) {
                    thrd_yield();
                }
                if(pred->type.load() == Node::LockType::WRITER) {
                    return;
                }
            }
            while(enclosingObject->readers_count.load() > 0) {
                thrd_yield();
            }
        }

        void unlock() {
            Node* node = myNode[enclosingObject].get();
            if(node->next.load() == nullptr) {
                if(enclosingObject->tail.compare_exchange_strong(node, nullptr)) {
                    return;
                }
                while(node->next.load() == nullptr) {
                    thrd_yield();
                }
            }

            node->next.load()->locked.store(false);
            node->next.store(nullptr);
        }
    };

    std::atomic<Node*> tail;
    std::atomic<uint64_t> readers_count; //TODO: change to SNZI
    static thread_local std::unordered_map<ReaderWriterLock*, MyUniquePtr> myNode;
    ReaderLock reader_Lock;
    WriterLock writer_Lock;

public:
    ReaderWriterLock() : reader_Lock{this}, writer_Lock{this} {
        tail.store(nullptr);
        readers_count.store(0);
    }

    ReaderLock& readerLock() {
        return reader_Lock;
    }

    WriterLock& writerLock() {
        return writer_Lock;
    }
};

#endif //SCALABLE_FAIR_READER_WRITER_LOCK_READERWRITERLOCK_H