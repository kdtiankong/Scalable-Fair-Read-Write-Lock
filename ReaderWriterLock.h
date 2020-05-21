//
// Created by mguzek on 15.05.2020.
//

#ifndef SCALABLE_FAIR_READER_WRITER_LOCK_READERWRITERLOCK_H
#define SCALABLE_FAIR_READER_WRITER_LOCK_READERWRITERLOCK_H

#include <atomic>
#include <threads.h>
#include <string>
#include <cstdint>

class ReaderWriterLock {
public:
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
    /*
    class MyUniquePtr {
        Node* ptr;
    public:
        MyUniquePtr() { ptr = nullptr; }
        explicit MyUniquePtr(Node* addr) : ptr{addr} { }
        MyUniquePtr& operator=(MyUniquePtr&& other) noexcept { ptr = std::exchange(other.ptr, nullptr); return *this; }
        Node* get() const { return ptr; }
        ~MyUniquePtr() { delete ptr; }
    };
     */

    class ReaderLock {
        ReaderWriterLock* const enclosingObject;

    public:
        explicit ReaderLock(ReaderWriterLock* owner) : enclosingObject{owner} {

        }

        void lock(const unsigned threadID, Node* myNode) {
            std::string toprintent = "\tThread " + std::to_string(threadID) + " enters readers.lock()\n";
            std::string toprintext = "\tThread " + std::to_string(threadID) + " exits readers.lock()\n";
            std::cerr << toprintent;

            enclosingObject->readers_count.fetch_add(1);
            if(enclosingObject->tail.load() == nullptr) {
                return;
            }

            myNode->type.store(Node::LockType::READER);
            Node* pred = enclosingObject->tail.exchange(myNode);
            if(pred == nullptr && enclosingObject->tail.compare_exchange_strong(myNode, nullptr)) {
                return;
            }

            if(pred != nullptr) {
                enclosingObject->readers_count.fetch_sub(1);
                myNode->locked.store(true);
                pred->next.store(myNode);
                std::string loop1b = "\t\t[Thread " + std::to_string(threadID) + "] #1 loop begin\n";
                std::string loop1e = "\t\t[Thread " + std::to_string(threadID) + "] #1 loop end\n";
                std::cerr << loop1b;
                while(myNode->locked.load()) { //sort of unfortunate chain of loops for chain of READERS
                    thrd_yield();
                }
                std::cerr << loop1e;
                enclosingObject->readers_count.fetch_add(1);
            }

            if(myNode->next.load() == nullptr) {
                if(enclosingObject->tail.load() == nullptr || enclosingObject->tail.compare_exchange_strong(myNode, nullptr)) {
                    return;
                }
                std::string loop2b = "\t\t[Thread " + std::to_string(threadID) + "] #2 loop begin\n";
                std::string loop2e = "\t\t[Thread " + std::to_string(threadID) + "] #2 loop end\n";
                std::cerr << loop2b;
                while(myNode->next.load() == nullptr) {
                    thrd_yield();
                }
                std::cerr << loop2e;
            }

            myNode->waitForNext = true;
            if(myNode->next.load()->type.load() == Node::LockType::READER) {
                myNode->next.load()->locked.store(false);
                myNode->waitForNext = false;
            }
            std::cerr << toprintext;
        }

        void unlock(const unsigned threadID, Node* myNode) {
            std::string toprintent = "Thread " + std::to_string(threadID) + " enters readers.unlock()\n";
            std::string toprintext = "Thread " + std::to_string(threadID) + " exits readers.unlock()\n";
            std::cerr << toprintent;

            enclosingObject->readers_count.fetch_sub(1);

            if(myNode->waitForNext) {
                std::string loop3b = "\t\t[Thread " + std::to_string(threadID) + "] #3 loop begin\n";
                std::string loop3e = "\t\t[Thread " + std::to_string(threadID) + "] #3 loop end\n";
                std::cerr << loop3b;
                while(myNode->next.load() == nullptr) {
                    thrd_yield();
                }
                std::cerr << loop3e;
                myNode->next.load()->locked.store(false);
            }
            myNode->next.store(nullptr);
            myNode->waitForNext = false;
            std::cerr << toprintext;
        }
    };

    class WriterLock {
        ReaderWriterLock* const enclosingObject;

    public:
        explicit WriterLock(ReaderWriterLock* owner) : enclosingObject{owner} {

        }

        void lock(const unsigned threadID, Node* myNode) {
            std::string toprintent = "Thread " + std::to_string(threadID) + " enters writers.lock()\n";
            std::string toprintext = "Thread " + std::to_string(threadID) + " exits writers.lock()\n";
            std::string toprintext2 = "Thread " + std::to_string(threadID) + " exits writers.lock() in 174\n";
            std::cerr << toprintent;

            myNode->type.store(Node::LockType::WRITER);
            Node* pred = enclosingObject->tail.exchange(myNode);
            std::string addr233 = "\t\t[Line 149][Thread " + std::to_string(threadID) + "] pred ="
                                  + std::to_string(reinterpret_cast<unsigned long>(pred)) + "\n";
            std::string addr3 = "\t\t[Line 149][Thread " + std::to_string(threadID) + "] tail ="
                                + std::to_string(reinterpret_cast<unsigned long>(enclosingObject->tail.load())) + "\n";
            std::string addr4 = "\t\t[Line 150][Thread " + std::to_string(threadID) + "] myNode ="
                                + std::to_string(reinterpret_cast<unsigned long>(myNode)) + "\n";
            std::cerr << addr233;
            std::cerr << addr3;
            std::cerr << addr4;

            if(pred != nullptr) {
                myNode->locked.store(true);
                std::string pprogress = "\t\t[Thread " + std::to_string(threadID) + "] stored its address in pred->next\n";
                pred->next.store(myNode);
                std::cerr << pprogress;
                std::string pprogress2 = "\t\t[Thread " + std::to_string(threadID) + "] pred->next.load() = "
                                         + std::to_string(reinterpret_cast<unsigned long>(pred->next.load())) +"\n";
                std::cerr << pprogress2;
                std::string loop4b = "\t\t[Thread " + std::to_string(threadID) + "] #4 loop begin\n";
                std::string loop4e = "\t\t[Thread " + std::to_string(threadID) + "] #4 loop end\n";
                std::string addr = "\t\t[Line 171][Thread " + std::to_string(threadID) + "] node address=" + std::to_string(reinterpret_cast<unsigned long>(myNode)) + "\n";
                std::string addr2 = "\t\t[Line 172][Thread " + std::to_string(threadID) + "] pred address=" + std::to_string(reinterpret_cast<unsigned long>(pred)) + "\n";
                std::cerr << addr;
                std::cerr << addr2;
                std::cerr << loop4b;
                while(myNode->locked.load()) {
                    thrd_yield();
                    std::cerr << "\t\t\tIn loop4\n";
                }
                std::cerr << loop4e;
                if(pred->type.load() == Node::LockType::WRITER) {
                    std::cerr << toprintext2;
                    return;
                }
            }
            std::string loop5b = "\t\t[Thread " + std::to_string(threadID) + "] #5 loop begin\n";
            std::string loop5e = "\t\t[Thread " + std::to_string(threadID) + "] #5 loop end\n";
            std::cerr << loop5b;
            while(enclosingObject->readers_count.load() > 0) {
                thrd_yield();
            }
            std::cerr << loop5e;
            std::cerr << toprintext;
        }

        void unlock(const unsigned threadID, Node* myNode) {

            std::string toprintent = "Thread " + std::to_string(threadID) + " enters writers.unlock()\n";
            std::string toprintext = "Thread " + std::to_string(threadID) + " exits writers.unlock()\n";
            std::string toprintext2 = "Thread " + std::to_string(threadID) + " exits writers.ulock() in 214\n";
            std::cerr << toprintent;

            std::string addr = "\t\t[Line 199][Thread " + std::to_string(threadID) + "] node address=" + std::to_string(reinterpret_cast<unsigned long>(myNode)) + "\n";
            std::cerr << addr;

            if(myNode->next.load() == nullptr) {
                std::string addr23 = "\t\t[Line 203][Thread " + std::to_string(threadID) + "] node address just after if entrance=" + 							std::to_string(reinterpret_cast<unsigned long>(myNode)) + "\n";
                std::cerr << addr23;

                if(enclosingObject->tail.compare_exchange_strong(myNode, nullptr)) {
                    std::cerr << toprintext2;
                    return;
                }

                std::string loop6b = "\t\t[Thread " + std::to_string(threadID) + "] #6 loop begin\n";
                std::string loop6e = "\t\t[Thread " + std::to_string(threadID) + "] #6 loop end\n";
                std::cerr << loop6b;
                std::string addr22 = "\t\t[Line 207][Thread " + std::to_string(threadID) + "] node address just before loop6 entrance=" + 						std::to_string(reinterpret_cast<unsigned long>(myNode)) + "\n";
                std::cerr << addr22;
                while(myNode->next.load() == nullptr) {
                    thrd_yield();
                    std::cerr << "\t\t\tIn loop6\n";
                    std::string addr34 = "\t\t[Loop 6][Thread " + std::to_string(threadID) + "] node address=" + std::to_string(reinterpret_cast<unsigned long>(myNode)) + "\n";
                    std::string addr35 = "\t\t[Loop 6][Thread " + std::to_string(threadID) + "] node->next.load()="
                                         + std::to_string(reinterpret_cast<unsigned long>(myNode->next.load())) + "\n";
                    std::cerr << addr34;
                    std::cerr << addr35;
                }
                std::cerr << loop6e;
            }

            myNode->next.load()->locked.store(false);
            std::cerr << toprintext;
        }
    };

    std::atomic<Node*> tail;
    std::atomic<uint64_t> readers_count; //TODO: change to SNZI
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