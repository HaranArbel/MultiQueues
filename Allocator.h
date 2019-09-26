////
//// Created by Haran Arbel on 2019-08-27.
////

#ifndef __ALLOCATOR__
#define __ALLOCATOR__

#include "recordmgr/record_manager.h"
#include "dAryMinHeap.h"
#include <map>
#include <pthread.h>
#include <mutex>

class Allocator{
private:
    static record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,Offer> * mgr;
    static std::mutex mgr_lock;

public:
    ~Allocator(){
        delete mgr;
    }

    static void init_allocator(int numOfThreads) {
        if (mgr == NULL){
            mgr = new record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,Offer>(numOfThreads, SIGQUIT);
        }
    }

    static Offer* allocate(int tid) {
        mgr_lock.lock();
        Offer* offer = mgr->template allocate<Offer>(tid);
        mgr_lock.unlock();
        return offer;
    }

    static void free(Offer* offer, int tid) {
        mgr_lock.lock();
        mgr->retire(tid,offer);
        mgr_lock.unlock();

    }

    static void enterQuiescentState(int tid) {
        mgr->enterQuiescentState(tid);
    }

    static void leaveQuiescentState(int tid) {
        mgr->leaveQuiescentState(tid);
    }

};

#endif