//
// Created by Haran Arbel on 2019-04-20.
//

#ifndef MULTIQUEUE_DARRYMINHEAP_H
#define MULTIQUEUE_DARRYMINHEAP_H

#include "Heap.h"
#include<iostream>
#include<cstdio>
#include<climits>
#include <sys/types.h>

#define PARENT(i,d) ((i - 1) / d)
#define CHILD(i,c,d) (d * i + c + 1)
#define D 8


struct Offer {
    Vertex* vertex;
    int dist;
};


class dAryMinHeap {

    public:
        dAryMinHeap(int capacity);
        Offer* extractMin();
        void insert(Offer *offer);
        bool isEmpty();
        Offer* findMin();
        ~dAryMinHeap();

    private:
        Heap* heap;
        int decreaseKey(int i, int dist);
        void minHeapify(int i);


};


#endif //MULTIQUEUE_DARRYMINHEAP_H
