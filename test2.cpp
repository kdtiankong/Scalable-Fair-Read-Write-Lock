//
// Created by mguzek on 15.05.2020.
//

//TEST CASE 2: 3 threads with SNZI tree of depth 1
//Run with: ./run.sh CS295/test3 -m 2 -y -x 10000

#include <threads.h>
#include "librace.h"
#include "ReaderWriterLock.h"

int sharedVariable = 5;

struct Args {
    ReaderWriterLock* rwlock;
    const unsigned threadID;
};

void func(Args* args)
{
    ReaderWriterLock* rwlock = args->rwlock;
    const unsigned threadID = args->threadID;
    ReaderWriterLock::Node myNode{};

    for(int i = 0 ; i < 1 ; ++i) {
        rwlock->readerLock().lock(threadID, &myNode);
        int tmp = load_32(&sharedVariable);
        rwlock->readerLock().unlock(threadID, &myNode);

        rwlock->writerLock().lock(threadID, &myNode);
        store_32(&sharedVariable, 1);
        rwlock->writerLock().unlock(threadID, &myNode);
    }
}

int user_main(int, char**) {
    ReaderWriterLock rwlock{1};

    Args args[3] = { {&rwlock, 0},
                     {&rwlock, 1},
                     {&rwlock, 2} };

    thrd_t thread1, thread2, thread3;

    thrd_create(&thread1, (thrd_start_t)func, args+0);
    thrd_create(&thread2, (thrd_start_t)func, args+1);
    thrd_create(&thread3, (thrd_start_t)func, args+2);

    thrd_join(thread1);
    thrd_join(thread2);
    thrd_join(thread3);
}