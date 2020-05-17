#include <iostream>
#include "ReaderWriterLock.h"

thread_local std::unordered_map<ReaderWriterLock*, std::unique_ptr<ReaderWriterLock::Node>> ReaderWriterLock::myNode{};

int main() {

    std::shared_ptr<int> p1;
    std::shared_ptr<int> p2{nullptr};
    std::cout << "p1 address: " << p1.get() << ", count: " << p1.use_count() << std::endl;
    std::cout << "p2 address: " << p2.get() << ", count: " << p2.use_count() << std::endl << std::endl;
    std::atomic_store(&p1, p2);
    std::cout << "p1 address: " << p1.get() << ", count: " << p1.use_count() << std::endl;
    std::cout << "p2 address: " << p2.get() << ", count: " << p2.use_count() << std::endl << std::endl;
    return 0;
}
