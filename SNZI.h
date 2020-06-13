//
// Created by mguzek on 21.05.2020.
//

#ifndef SCALABLE_FAIR_READER_WRITER_LOCK_SNZI_H
#define SCALABLE_FAIR_READER_WRITER_LOCK_SNZI_H

#include <atomic>
#include <cstdint>
#include <cmath>
#include "model-assert.h"

class SNZI {
    struct Node {
        Node* const parent;
                                    //(bit ranges from left to right)
        std::atomic<uint64_t> X;    //hierarchical node: 32 bits for counter, 31 bits for version number, 1 bit if "counter == 1/2"
                                    //root node: 32 bits for counter, 31 bits for version number, 1 announce bit
        std::atomic<uint64_t> I;    //only for the root node: 63 bits for version number, 1 bit for indicator

        Node(Node* parent) : parent{parent} {
            X.store(0, std::memory_order_release);
            I.store(0, std::memory_order_release);
        }

        //----------------------------------------------------------------------------------------------------------
        //helper methods for bit manipulation
        static uint64_t isolateBits(const uint64_t value, unsigned noBits, unsigned fromBit) {
            if(fromBit >= 64) return 0;

            uint64_t mask = ((uint64_t(1) << noBits) - uint64_t(1)) << fromBit;
            return (value & mask) >> fromBit;
        }

        static void setX(uint64_t& x, uint32_t counter, uint32_t version, uint8_t flag) {
            version &= ~(1u << 31u);            //clearing 32th bit of 31-bit value
            uint64_t counter64 = counter;
            counter64 <<= 32u;
            uint64_t version64 = version;
            version64 <<= 1u;
            x = (counter64 | version64 | flag);
        }

        static void setI(uint64_t& i, uint64_t version, uint8_t flag) {
            version <<= 1u;
            i = (version | flag);
        }

        static uint32_t extractCounterFromX(const uint64_t x) {
            return isolateBits(x, 32, 32);
        }
        static uint32_t extractVersionFromX(const uint64_t x) {
            return isolateBits(x, 31, 1);
        }
        static uint8_t extractFlagFromX(const uint64_t x) {
            return isolateBits(x, 1, 0);
        }

        static uint64_t r_extractVersionFromI(const uint64_t i) {
            return isolateBits(i, 63, 1);
        }
        static uint8_t r_extractFlagFromI(const uint64_t i) {
            return isolateBits(i, 1, 0);
        }
        //----------------------------------------------------------------------------------------------------------

        void hierarchicalNodeArrive() {
            bool succ = false;
            unsigned undoArr = 0;
            while(!succ) {
                uint64_t x = X.load(std::memory_order_acquire);
                uint32_t x_c = extractCounterFromX(x);
                uint8_t x_f = extractFlagFromX(x);
                if(x_c >= 1 && !x_f) {
                    uint64_t new_x = 0;
                    setX(new_x, x_c+1, extractVersionFromX(x), 0);
                    uint64_t copy_of_x = x; //copying original value since CAS might modify it in case of a failure
                    if(X.compare_exchange_strong(copy_of_x, new_x, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        succ = true;
                    }
                }
                if(x_c == 0 && !x_f) {
                    uint64_t new_x = 0;
                    setX(new_x, 0, extractVersionFromX(x)+1, 1);
                    uint64_t copy_of_x = x;
                    if(X.compare_exchange_strong(copy_of_x, new_x, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        succ = true;
                        x = new_x;
                    }
                }
                if(extractFlagFromX(x)) {
                    parent->arrive();
                    uint64_t new_x = 0;
                    setX(new_x, 1, extractVersionFromX(x), 0);
                    uint64_t copy_of_x = x;
                    if(!X.compare_exchange_strong(copy_of_x, new_x, std::memory_order_acq_rel, std::memory_order_relaxed)) {
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
            uint64_t x = X.load(std::memory_order_acquire);
            while(true) {
                uint64_t x_c = extractCounterFromX(x);
                MODEL_ASSERT(x_c >= 1);
                uint64_t new_x = 0;
                setX(new_x, x_c-1, extractVersionFromX(x), 0);
                if(X.compare_exchange_strong(x, new_x, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    if(x_c == 1) {
                        parent->depart();
                    }
                    return;
                }
            }
        }

        void rootNodeArrive() {
            uint64_t x = X.load(std::memory_order_acquire);
            uint64_t new_x;
            do {
                uint64_t x_c = extractCounterFromX(x);
                if(x_c == 0) {
                    setX(new_x, 1, extractVersionFromX(x)+1, 1);
                }
                else {
                    new_x = x;
                    setX(new_x, x_c+1, extractVersionFromX(x), extractFlagFromX(x));
                }
            } while(!X.compare_exchange_strong(x, new_x, std::memory_order_acq_rel, std::memory_order_relaxed));

            if(extractFlagFromX(new_x)) {
                uint64_t i = I.load(std::memory_order_acquire);
                uint64_t new_i;
                do {
                    setI(new_i, r_extractVersionFromI(i)+1, 1);
                } while(!I.compare_exchange_strong(i, new_i, std::memory_order_acq_rel, std::memory_order_relaxed));

                uint64_t new_x_withNoAnnounceBit = new_x & ~(uint64_t(1));
                X.compare_exchange_strong(new_x, new_x_withNoAnnounceBit, std::memory_order_acq_rel, std::memory_order_relaxed);
            }
        }

        void rootNodeDepart() {
            uint64_t x = X.load(std::memory_order_acquire);
            while(true) {
                uint64_t x_c = extractCounterFromX(x);
                MODEL_ASSERT(x_c >= 1);
                uint64_t x_v = extractVersionFromX(x);
                uint64_t new_x;
                setX(new_x, x_c-1, x_v, 0);
                if(X.compare_exchange_strong(x, new_x, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    if(x_c >= 2) {
                        return;
                    }

                    uint64_t i = I.load(std::memory_order_acquire);
                    if(extractVersionFromX(X.load(std::memory_order_acquire)) != x_v) {
                        return;
                    }
                    uint64_t new_i;
                    setI(new_i, r_extractVersionFromI(i)+1, 0);
                    I.compare_exchange_strong(i, new_i, std::memory_order_acq_rel, std::memory_order_relaxed);
                    return;
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
    SNZI(unsigned depth) :
            root{new Node(nullptr)},
            nodeCount{0},
            nodes{new Node*[static_cast<uint64_t>(std::pow(2, depth+1)-1)]},
            leafCount{0},
            leaves{new Node*[static_cast<uint64_t>(std::pow(2, depth))]} {
        MODEL_ASSERT(depth >= 1);
        nodes[nodeCount++] = root;
        initializeTree(root, depth-1);
        initializeTree(root, depth-1);

        auto calculatedNoOfNodes = static_cast<uint64_t>(std::pow(2, depth+1)-1);
        auto calculatedNoOfLeaves = std::pow(2, depth);
        MODEL_ASSERT(nodeCount == calculatedNoOfNodes);
        MODEL_ASSERT(leafCount == calculatedNoOfLeaves);
    }

    void arrive(unsigned leaf) {
        leaves[leaf % leafCount]->arrive();
    }

    void depart(unsigned leaf) {
        leaves[leaf % leafCount]->depart();
    }

    bool query() const {
        return SNZI::Node::r_extractFlagFromI(root->I.load(std::memory_order_seq_cst));
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