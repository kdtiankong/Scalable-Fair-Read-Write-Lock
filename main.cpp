//
// Created by mguzek on 15.05.2020.
//

#include <iostream>
#include <bitset>
#include <thread>
//#include "librace.h"
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

    for(int i = 0 ; i < 5 ; ++i) {
        rwlock->readerLock().lock(threadID, &myNode);
        //int tmp = load_32(&sharedVariable);
        rwlock->readerLock().unlock(threadID, &myNode);

        rwlock->writerLock().lock(threadID, &myNode);
        //store_32(&sharedVariable, 1);
        rwlock->writerLock().unlock(threadID, &myNode);

        rwlock->readerLock().lock(threadID, &myNode);
        //tmp = load_32(&sharedVariable);
        rwlock->readerLock().unlock(threadID, &myNode);
    }
}

//int user_main(int, char**) {
int main(int, char**) {

    ReaderWriterLock rwlock{};

    Args args[3] = { {&rwlock, 1},
                     {&rwlock, 2},
                     {&rwlock, 3} };
/*
    thrd_t thread1, thread2, thread3;

    thrd_create(&thread1, (thrd_start_t)func, args+0);
    thrd_create(&thread2, (thrd_start_t)func, args+1);
    thrd_create(&thread3, (thrd_start_t)func, args+2);

    thrd_join(thread1);
    thrd_join(thread2);
    thrd_join(thread3);
    */

    std::thread t1(func, args+0);
    std::thread t2(func, args+1);

    t1.join();
    t2.join();


/*
    int depth = 5;
    SNZI snzi(depth);
    std::cout << std::boolalpha;
    std::cout << "leafCount=" << snzi.leafCount << "\n";
    std::cout << "nodeCount=" << snzi.nodeCount << "\n\n";

    std::cout << "1query=" << std::bitset<64>(snzi.root->I.load()) << " " << snzi.query() << "\n";
    snzi.arrive(0);
    std::cout << "2query=" << std::bitset<64>(snzi.root->I.load()) << " " << snzi.query() << "\n";
    snzi.depart(0);
    std::cout << "3query=" << std::bitset<64>(snzi.root->I.load()) << " " << snzi.query() << "\n";
    snzi.arrive(1);
    snzi.arrive(1);
    snzi.arrive(1);
    std::cout << "4query=" << std::bitset<64>(snzi.root->I.load()) << " " << snzi.query() << "\n";
    snzi.depart(1);
    snzi.depart(1);
    snzi.depart(1);
    std::cout << "5query=" << std::bitset<64>(snzi.root->I.load()) << " " << snzi.query() << "\n";
    snzi.arrive(3);
    snzi.arrive(2);
    snzi.depart(2);
    snzi.depart(3);
    std::cout << "6query=" << std::bitset<64>(snzi.root->I.load()) << " " << snzi.query() << "\n";
    //snzi.depart(1);
    //std::cout << "5query=" << std::bitset<64>(snzi.root->I.load()) << " " << snzi.query() << "\n";
    //snzi.arrive(1);
    //snzi.arrive(2);
    //snzi.arrive(2);
    //std::cout << "6query=" << std::bitset<64>(snzi.root->I.load()) << " " << snzi.query() << "\n";
*/


    /*
    uint64_t allOnes = 0;
    std::cout << std::bitset<64>(allOnes) << "\n";

    uint64_t counter = 0b11111111111111111111111111111111;
    SNZI::Node::setCounterInX(allOnes, counter);
    SNZI::Node::setVersionInX(allOnes, 0);
    SNZI::Node::setFlagInX(allOnes, 1);
    std::cout << std::bitset<64>(allOnes) << "\n\n";

    counter -= 2;
    SNZI::Node::setCounterInX(allOnes, counter);
    std::cout << std::bitset<64>(allOnes) << "\n\n";

    counter -= 1;
    SNZI::Node::setCounterInX(allOnes, counter);
    std::cout << std::bitset<64>(allOnes) << "\n\n";

    uint64_t version = 0b1111111111111111111111111111111;
    SNZI::Node::setVersionInX(allOnes, version);
    std::cout << std::bitset<64>(allOnes) << "\n\n";

    SNZI::Node::setFlagInX(allOnes, 0);
    std::cout << std::bitset<64>(allOnes) << "\n\n";

    std::cout << std::bitset<64>(SNZI::Node::extractCounterFromX(allOnes)) << "\n";
    std::cout << std::bitset<64>(SNZI::Node::extractVersionFromX(allOnes)) << "\n";
    std::cout << std::bitset<64>(SNZI::Node::extractFlagFromX(allOnes)) << "\n";
    SNZI::Node::setFlagInX(allOnes, 1);
    std::cout << std::bitset<64>(SNZI::Node::extractFlagFromX(allOnes)) << "\n\n";

    std::cout << std::bitset<64>(SNZI::Node::r_extractVersionFromI(allOnes)) << "\n";
    std::cout << std::bitset<64>(SNZI::Node::r_extractFlagFromI(allOnes)) << "\n\n";

    uint64_t versionr = 0b011111111111111111111111111111001111111111111111111111111111110;
    SNZI::Node::r_setVersionInI(allOnes, versionr);
    std::cout << std::bitset<64>(allOnes) << "\n";
    SNZI::Node::r_setFlagInI(allOnes, 0);
    std::cout << std::bitset<64>(allOnes) << "\n\n";

    uint64_t neww = SNZI::Node::r_extractVersionFromI(allOnes);
    neww += 1;
    SNZI::Node::r_setVersionInI(allOnes, neww);
    std::cout << std::bitset<64>(allOnes) << "\n\n";
     */
}