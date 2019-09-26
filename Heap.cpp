//
// Created by Haran Arbel on 2019-04-20.
//

#include "Heap.h"


Heap::Heap(int capacity, int d) {
    this->heap_size = 0;
    this->d = d;
    this->elements = new Offer*[capacity];
    this->capacity = capacity;
    for (int i=0; i<this->capacity; i++){
        this->elements[i] = NULL;
    }
}


void Heap::increase_size() {
    this->capacity = this->capacity * 2;
    Offer** newArray = new Offer*[this->capacity];
    for (int i=0; i<this->capacity; i++){
        newArray[i] = NULL;
    }
    for(int i=0; i < this->capacity / 2; i++) {
        newArray[i] = this->elements[i];
    }
    delete [] this->elements;
    this->elements = newArray;
}

Heap::~Heap() {
    delete [] this->elements;
}