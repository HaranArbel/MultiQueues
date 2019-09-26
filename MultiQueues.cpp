//
// Created by Haran Arbel on 2019-04-20.
//

#include "MultiQueues.h"

MultiQueues::MultiQueues(int c, int p) {
    this->c = c;
    this->p = p;
    this->numOfQueues = c*p;
    this->queues = new dAryMinHeap*[numOfQueues];
    this->locks = new std::mutex*[numOfQueues];
    this->init();
    this->numOffers = 0;
    srand(time(0));

}


void MultiQueues:: init() {
    for(int i=0 ; i < this->numOfQueues; i++) {
        this->queues[i] = new dAryMinHeap(QUEUE_CAPACITY);
        this->locks[i] = new std::mutex();
    }

}
bool MultiQueues::is_empty(){
    if (this->numOffers == 0){
        return true;
    }
    return false;
}

Offer* MultiQueues::insert(Vertex* vertex, int dist, int tid) {

    Allocator::enterQuiescentState(tid);

    Offer* offer = Allocator::allocate(tid);
    offer->dist = dist;
    offer->vertex = vertex;

    this->numOffers++;

    int queueIndex;
    do {
        queueIndex = getRandomQueueIndex();
    } while (!locks[queueIndex]->try_lock());

    this->queues[queueIndex]->insert(offer);
    locks[queueIndex]->unlock();

    Allocator::leaveQuiescentState(tid);
    return offer;
}

bool MultiQueues::deleteMin(Offer *out, int tid) {

    Allocator::enterQuiescentState(tid);

    int minIndex = -1;

    int num_loops = 0;
minLoop:
    do {
        num_loops++;

        int i = this->getRandomQueueIndex();
        int j = this->getRandomQueueIndex();

        if (this->numOffers == 0){
            Allocator::leaveQuiescentState(tid);
            return false;
        }
        if(this->queues[i]->isEmpty() && this->queues[j]->isEmpty()){
            continue;
        }

        else if(!this->queues[i]->isEmpty() && this->queues[j]->isEmpty())
            minIndex = i;
        else if(this->queues[i]->isEmpty() && !this->queues[j]->isEmpty())
            minIndex = j;
        else {
            if (this->queues[i]->findMin() < this->queues[j]->findMin())
                minIndex = i;
            else
                minIndex = j;
        }
    }
    while(minIndex == -1 || !this->locks[minIndex]->try_lock());


    if (this->queues[minIndex]->isEmpty()) {
        this->locks[minIndex]->unlock();
        goto minLoop;
    }

    Offer* min_offer = this->queues[minIndex]->extractMin();

    this->locks[minIndex]->unlock();
    if (min_offer){
        this->numOffers--;
    }

    out->vertex = min_offer->vertex;
    out->dist = min_offer->dist;

    Allocator::free(min_offer, tid);

    Allocator::leaveQuiescentState(tid);
    return true;

}


int MultiQueues::getRandomQueueIndex() {
    int random = rand() % this->numOfQueues;
    return random;
}

MultiQueues::~MultiQueues() {
    for(int i = 0; i < this->numOfQueues; i++) {
        delete this->queues[i];
        delete this->locks[i];
    }
    delete [] this->queues;
    delete [] this->locks;
    delete seed;
}
