//
// Created by mguzek on 15.05.2020.
//

#include <iostream>
#include <string>
#include "librace.h"
#include "ReaderWriterLock.h"

int sharedVariable = 5;

void func(ReaderWriterLock* rwlock)
{
    //ReaderWriterLock* rwlock = args->rwlock;
    //const unsigned threadID = args->threadInd;
    ReaderWriterLock::Node myNode{};// = new ReaderWriterLock::Node{};

    //std::cout << "Begin function for thread = " << threadID << "\n";
    for(int i = 0 ; i < 5 ; ++i) {
        //std::string toprint = "### [Thread " + std::to_string(threadID) + "] i = " + std::to_string(i) + "\n";
        //std::string pprogress = "### [Thread " + std::to_string(threadID) + "] modified sharedVariable and is about to leave CS\n";
        //std::cout<<toprint;

        rwlock->readerLock().lock(&myNode);
        int tmp = load_32(&sharedVariable);
        rwlock->readerLock().unlock(&myNode);

        rwlock->writerLock().lock(&myNode);
        store_32(&sharedVariable, 1);
        //std::cout << pprogress;
        rwlock->writerLock().unlock(&myNode);
    }
    //std::cout << "End function for thread = " << threadID << "\n";
    //delete myNode;
}

int user_main(int, char**) {
    ReaderWriterLock rwlock{};

    thrd_t thread1, thread2, thread3;

    thrd_create(&thread1, (thrd_start_t)func, &rwlock);
    thrd_create(&thread2, (thrd_start_t)func, &rwlock);
    thrd_create(&thread3, (thrd_start_t)func, &rwlock);

    thrd_join(thread1);
    thrd_join(thread2);
    thrd_join(thread3);
}