//
// Created by Haran Arbel on 2019-04-20.
//

#ifndef MULTIQUEUE_MULTIQUEUE_H
#define MULTIQUEUE_MULTIQUEUE_H


#include "dAryMinHeap.h"
#include "Allocator.h"
#include <mutex>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <thread>


#define QUEUE_CAPACITY 2048
using namespace std;



class MultiQueues {
    int c;
    int p;
    unsigned int *seed = new unsigned int[1];
    int numOfQueues;
    atomic<int> numOffers;
    dAryMinHeap** queues;
    std::mutex** locks;

    public:
        MultiQueues(int c, int p);
        Offer* insert(Vertex* vertex, int dist, int tid);
        bool deleteMin(Offer *out, int tid);
        void init();
        int getRandomQueueIndex();
        bool is_empty();
        ~MultiQueues();

};



#endif //MULTIQUEUE_MULTIQUEUE_H
