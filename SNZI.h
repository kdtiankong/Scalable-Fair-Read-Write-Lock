//
// Created by mguzek on 21.05.2020.
//

#ifndef SCALABLE_FAIR_READER_WRITER_LOCK_SNZI_H
#define SCALABLE_FAIR_READER_WRITER_LOCK_SNZI_H

#include <atomic>
#include <cstdint>
#include <cmath>
//#include "model-assert.h"

class SNZI {
public: //TODO temporary
    struct Node {
        Node* const parent;
                                    //(bit ranges from left to right)
        std::atomic<uint64_t> X;    //hierarchical node: 32 bits for counter, 31 bits for version number, 1 bit if "counter == 1/2"
                                    //root node: 32 bits for counter, 31 bits for version number, 1 announce bit
        std::atomic<uint64_t> I;    //only for the root node: 63 bits for version number, 1 bit for indicator

        Node(Node* parent) : parent{parent} {
            X.store(0);
            I.store(0);
        }

        //----------------------------------------------------------------------------------------------------------
        //helper methods for bit manipulation
        static uint64_t isolateBits(const uint64_t value, unsigned noBits, unsigned fromBit) {
            if(fromBit >= 64) return 0;

            uint64_t mask = ((uint64_t(1) << noBits) - uint64_t(1)) << fromBit;
            return (value & mask) >> fromBit;
        }

        static uint64_t isolateOriginalBits(const uint64_t value, unsigned noBits, unsigned fromBit) {
            if(fromBit >= 64) return 0;

            uint64_t mask = ((uint64_t(1) << noBits) - uint64_t(1)) << fromBit;
            return value & mask;
        }

        static void setBitGroup(uint64_t& value, unsigned fromBit, const uint64_t bitMask, unsigned noBitsInMask) {
            if(fromBit >= 64) return;

            uint64_t mask = (uint64_t(1) << noBitsInMask) - uint64_t(1);
            uint64_t lastXBits = bitMask & mask;

            uint64_t firstPart = isolateOriginalBits(value, 64-(fromBit+noBitsInMask), fromBit+noBitsInMask);
            uint64_t middlePart = (lastXBits << fromBit);
            uint64_t lastPart = isolateOriginalBits(value, fromBit, 0);

            value = firstPart + middlePart + lastPart;
        }

        static uint64_t extractCounterFromX(const uint64_t x) {
            return isolateBits(x, 32, 32);
        }
        static uint64_t extractVersionFromX(const uint64_t x) {
            return isolateBits(x, 31, 1);
        }
        static uint64_t extractFlagFromX(const uint64_t x) {
            return isolateBits(x, 1, 0);
        }
        static void setCounterInX(uint64_t& x, const uint64_t counter) {
            setBitGroup(x, 32, counter, 32);
        }
        static void setVersionInX(uint64_t& x, const uint64_t version) {
            setBitGroup(x, 1, version, 31);
        }
        static void setFlagInX(uint64_t& x, const uint64_t flag) {
            setBitGroup(x, 0, flag, 1);
        }

        static uint64_t r_extractVersionFromI(const uint64_t i) {
            return isolateBits(i, 63, 1);
        }
        static uint64_t r_extractFlagFromI(const uint64_t i) {
            return isolateBits(i, 1, 0);
        }
        static void r_setVersionInI(uint64_t& i, const uint64_t version) {
            setBitGroup(i, 1, version, 63);
        }
        static void r_setFlagInI(uint64_t& i, const uint64_t flag) {
            setBitGroup(i, 0, flag, 1);
        }
        //----------------------------------------------------------------------------------------------------------

        void hierarchicalNodeArrive() {
            bool succ = false;
            unsigned undoArr = 0;
            while(!succ) {
                uint64_t x = X.load();
                uint64_t x_c = extractCounterFromX(x);
                if(x_c >= 1) {
                    uint64_t new_x = 0;
                    setCounterInX(new_x, x_c+1);
                    setVersionInX(new_x, extractVersionFromX(x));
                    uint64_t copy_of_x = x;
                    if(X.compare_exchange_strong(copy_of_x, new_x)) {
                        succ = true;
                    }
                }
                if(x_c == 0) {
                    uint64_t new_x = 0;
                    setVersionInX(new_x, extractVersionFromX(x)+1);
                    setFlagInX(new_x, 1);
                    uint64_t copy_of_x = x;
                    if(X.compare_exchange_strong(copy_of_x, new_x)) {
                        succ = true;
                        x = new_x;
                    }
                }
                if(extractFlagFromX(x)) {
                    parent->arrive();
                    uint64_t new_x = 0;
                    setCounterInX(new_x, 1);
                    setVersionInX(new_x, extractVersionFromX(x));
                    uint64_t copy_of_x = x;
                    if(!X.compare_exchange_strong(copy_of_x, new_x)) {
                        ++undoArr;
                    }
                }
            }
            while(undoArr > 0) {
                parent->depart();
                --undoArr;
            }
        }

        void hierarchicalNodeDepart() {
            while(true) {
                uint64_t x = X.load();
                //MODEL_ASSERT(extractCounterFromX(x) >= 1);
                uint64_t new_x = 0;
                uint64_t x_c = extractCounterFromX(x);
                setCounterInX(new_x, x_c-1);
                setVersionInX(new_x, extractVersionFromX(x));
                if(X.compare_exchange_strong(x, new_x)) {
                    if(x_c == 1) {
                        parent->depart();
                    }
                    return;
                }
            }
        }

        void rootNodeArrive() {
            uint64_t x = X.load();
            //std::cout << "A-BEFORE: " << std::bitset<64>(x) << "\n";
            uint64_t new_x;
            do {
                uint64_t x_c = extractCounterFromX(x);
                if(x_c == 0) {
                    setCounterInX(new_x, 1);
                    setFlagInX(new_x, 1);
                    setVersionInX(new_x, extractVersionFromX(x)+1);
                }
                else {
                    new_x = x;
                    setCounterInX(new_x, x_c+1);
                }
            } while(!X.compare_exchange_strong(x, new_x));

            //std::cout << "A-1AFTER: " << std::bitset<64>(X.load()) << "\n";

            if(extractFlagFromX(new_x)) {
                uint64_t i = I.load();
                uint64_t new_i;
                r_setFlagInI(new_i, 1);
                do {
                    r_setVersionInI(new_i, r_extractVersionFromI(i)+1);
                } while(!I.compare_exchange_strong(i, new_i));

                uint64_t new_x_withNoAnnounceBit = new_x;
                setFlagInX(new_x_withNoAnnounceBit, 0);
                X.compare_exchange_strong(new_x, new_x_withNoAnnounceBit);
            }
            //std::cout << "A-2AFTER: " << std::bitset<64>(X.load()) << "\n";
        }

        void rootNodeDepart() {
            uint64_t x = X.load();
            //std::cout << "D-BEFORE: " << std::bitset<64>(x) << "\n";
            while(true) {
                uint64_t x_c = extractCounterFromX(x);
                //MODEL_ASSERT(x_c >= 1);
                uint64_t x_v = extractCounterFromX(x);
                uint64_t new_x;
                setCounterInX(new_x, x_c-1);
                setFlagInX(new_x, 0);
                setVersionInX(new_x, x_v);
                if(X.compare_exchange_strong(x, new_x)) {
                    //std::cout << "D- AFTER: " << std::bitset<64>(X.load()) << "\n";
                    if(x_c >= 2) {
                        return;
                    }

                    uint64_t i = I.load();
                    if(extractVersionFromX(X.load()) != x_v) {
                        return;
                    }
                    uint64_t new_i;
                    r_setVersionInI(new_i, r_extractVersionFromI(i)+1);
                    r_setFlagInI(new_i, 0);
                    if(I.compare_exchange_strong(i, new_i)) {
                        return;
                    }
                }
            }
        }

        void arrive() {
            if(parent == nullptr) {
                rootNodeArrive();
            }
            else {
                hierarchicalNodeArrive();
            }
        }

        void depart() {
            if(parent == nullptr) {
                rootNodeDepart();
            }
            else {
                hierarchicalNodeDepart();
            }
        }
    };

    void initializeTree(Node* parent, int depth) {
        if(depth == 0) {
            Node* leaf = new Node(parent);
            leaves[leafCount++] = leaf;
            nodes[nodeCount++] = leaf;
        }
        else {
            Node* node = new Node(parent);
            nodes[nodeCount++] = node;
            initializeTree(node, depth - 1);
            initializeTree(node, depth - 1);
        }
    }

    Node* root;
    unsigned nodeCount;
    Node** nodes;
    unsigned leafCount;
    Node** leaves;

public:
    SNZI(int depth) :
                        root{new Node(nullptr)},
                        nodeCount{0},
                        nodes{new Node*[static_cast<uint64_t>(std::pow(2, depth+1)-1)]},
                        leafCount{0},
                        leaves{new Node*[static_cast<uint64_t>(std::pow(2, depth))]} {
        nodes[nodeCount++] = root;
        initializeTree(root, depth-1);
        initializeTree(root, depth-1);
        /*
        auto calculatedNoOfNodes = static_cast<uint64_t>(std::pow(2, depth+1)-1);
        auto calculatedNoOfLeaves = std::pow(2, depth);
        MODEL_ASSERT(nodeCount == calculatedNoOfNodes);
        MODEL_ASSERT(leafCount == calculatedNoOfLeaves);
         */
    }

    void arrive(unsigned leaf) {
        leaves[leaf % leafCount]->arrive();
    }

    void depart(unsigned leaf) {
        leaves[leaf % leafCount]->depart();
    }

    bool query() const {
        return SNZI::Node::r_extractFlagFromI(root->I.load());
    }

    ~SNZI() {
        for(int i = 0 ; i < nodeCount ; ++i) {
            delete nodes[i];
        }
        delete [] nodes;
        delete [] leaves;
    }
};

#endif //SCALABLE_FAIR_READER_WRITER_LOCK_SNZI_H
