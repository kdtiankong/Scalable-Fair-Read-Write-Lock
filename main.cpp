//
// Created by mguzek on 15.05.2020.
//

#include <iostream>
//#include "librace.h"
#include <thread>
#include "ReaderWriterLock.h"

thread_local std::unordered_map<ReaderWriterLock*, ReaderWriterLock::MyUniquePtr> ReaderWriterLock::myNode{};

//ReaderWriterLock showtime;
int sharedVariable = 6;

struct Args {
    ReaderWriterLock* rwlock;
    int threadInd;
};

void func(Args* args)
{
    ReaderWriterLock* rwlock = args->rwlock;
    int threadID = args->threadInd;

    std::cout << "Begin function for thread = " << threadID << "\n";
    for(int i = 0 ; i < 1 ; ++i) {
        rwlock->readerLock().lock();
        int tmp = sharedVariable;
        rwlock->readerLock().unlock();
        //rwlock->writerLock().lock();
        //store_32(&sharedVariable, 1);
        //rwlock->writerLock().unlock();
    }
    std::cout << "End function for thread = " << threadID << "\n";
}

int main(int, char**) {
    ReaderWriterLock showtime;

    Args args[2] = {   {&showtime, 0},
                       {&showtime, 1} };
/*
    thrd_t thread1, thread2;

    thrd_create(&thread1, (thrd_start_t)func, args+0);
    thrd_create(&thread2, (thrd_start_t)func, args+1);

    thrd_join(thread1);
    thrd_join(thread2);
    */
    std::thread t1(func, args+0);
    std::thread t2(func, args+1);

    t1.join();
    t2.join();
}