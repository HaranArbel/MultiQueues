//
// Created by Haran Arbel on 2019-04-20.
//

#include "dAryMinHeap.h"
#include "Heap.h"//todo remove
#include "Allocator.h"

dAryMinHeap::dAryMinHeap(int capacity) {
    this->heap = new ::Heap(capacity, D);
}


void dAryMinHeap:: insert(Offer *offer) {

    if(this->heap->heap_size >= this->heap->capacity) {
        this->heap->increase_size();

    }

    heap->heap_size++;

    int dist = offer->dist;
    heap->elements[heap->heap_size - 1] = offer;
    offer->dist = INT_MAX;
    int i = this->decreaseKey(heap->heap_size - 1, dist);
    heap->elements[i] = offer;
    offer->dist=dist;

}


int dAryMinHeap:: decreaseKey(int i, int dist) {

    if (dist > heap->elements[i]->dist) {
        std::cerr << "new key is larger than current key" << std::endl;
        exit(-1);
    }

    while (i > 0 && heap->elements[PARENT(i,heap->d)]->dist > dist) {
        heap->elements[i] = heap->elements[PARENT(i,heap->d)];
        heap->elements[PARENT(i,heap->d)] = nullptr; //todo check

        i = PARENT(i,heap->d);
    }

    return i;
}


void dAryMinHeap:: minHeapify(int i){
    int smallest = i;
    int basechild = CHILD(i, 0, this->heap->d);

    for (int k = 0; k < this->heap->d; k++) {
        int child = basechild+k;
        if (child < this->heap->heap_size && this->heap->elements[child]->dist < this->heap->elements[smallest]->dist)
            smallest = child;
    }

    if (smallest != i) {
        std::swap(this->heap->elements[i],this->heap->elements[smallest]);

        this->minHeapify(smallest);
    }
}

Offer* dAryMinHeap:: extractMin() {

    Offer* min_offer = this->heap->elements[0];
    heap->elements[0] = heap->elements[heap->heap_size - 1];
    heap->elements[heap->heap_size - 1]=nullptr; //todo check
    heap->heap_size--;

    this->minHeapify(0);

    return min_offer;
}

bool dAryMinHeap::isEmpty(){
    return this->heap->heap_size == 0;
}

Offer* dAryMinHeap::findMin() {
    if(this->isEmpty()) {
        return NULL;
    }
    return this->heap->elements[0];
}

dAryMinHeap::~dAryMinHeap() {
    delete this->heap;
}

