//
// Created by Haran Arbel on 2019-04-20.
//

#ifndef MULTIQUEUE_HEAP_H
#define MULTIQUEUE_HEAP_H

#include <climits>
#include "Graph.h"


class Heap {
    public:
        Heap(int capacity, int d);
        struct Offer** elements;
        int d;
        int heap_size;
        int capacity;
        ~Heap();
        void increase_size();
};


#endif //MULTIQUEUE_HEAP_H
