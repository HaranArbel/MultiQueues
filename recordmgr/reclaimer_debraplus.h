/**
 * Implementation of a Record Manager with several memory reclamation schemes.
 * This file provides a Reclaimer plugin for the Record Manager.
 * Specifically, it provides an implementation of DEBRA+.
 * 
 * NOTE: This implementation is more NUMA friendly than the implementation
 * described in the paper.
 *
 * Copyright (C) 2016 Trevor Brown
 * Contact (tabrown [at] cs [dot] toronto [dot edu]) with any questions or comments.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RECLAIM_EPOCH_CRASHRECOV_H
#define	RECLAIM_EPOCH_CRASHRECOV_H

#include <cassert>
#include <iostream>
#include "machineconstants.h"
#include "globals.h"
#include "blockbag.h"
#include "allocator_interface.h"
#include "reclaimer_interface.h"
#include "arraylist.h"
#include "hashtable.h"
#include "record_manager_single_type.h"
using namespace std;
using namespace hashset_namespace;

template <typename T = void, class Pool = pool_interface<T> >
class reclaimer_debraplus : public reclaimer_interface<T, Pool> {
private:
#define EPOCH_INCREMENT 2
#define BITS_EPOCH(ann) ((ann)&~(EPOCH_INCREMENT-1))
#define QUIESCENT(ann) ((ann)&1)
#define GET_WITH_QUIESCENT(ann) ((ann)|1)
// the following threshold allows a process to accumulate about 768 objects in each epoch bag
// (3*BLOCK_SIZE=768, but there are other things that inflate bag size slightly, such as
//  the fact that a thread can do n operations before it successfully neutralizes each thread
//  and can advance the epoch.)
#define NEUTRALIZE_THRESHOLD_IN_BLOCKS 4
// maximum number of objects that can be simultaneously protected by calls to qProtect()
#define MAX_PROTECT_EVEN_IF_QUIESCENT 7

#define MIN_OPS_BEFORE_READ_CR 20
#define NUMBER_OF_EPOCH_BAGS_CR 3
    // for epoch based reclamation
    volatile long epoch;
    atomic_long *announcedEpoch;        // announcedEpoch[tid*PREFETCH_SIZE_WORDS] = bits 1..end contain the last epoch seen by thread tid, and bit 0 indicates quiescence
    long *checked;                      // checked[tid*PREFETCH_SIZE_WORDS] = how far we've come in checking the announced epochs of other threads
    blockbag<T> **epochbags;            // epochbags[NUMBER_OF_EPOCH_BAGS*tid+0..NUMBER_OF_EPOCH_BAGS*tid+(NUMBER_OF_EPOCH_BAGS-1)] are epoch bags for thread tid.
    blockbag<T> **currentBag;           // pointer to current epoch bag for each process
    long *index;                        // index of currentBag in epochbags for each process
    // note: oldest bag is number (index+1)%NUMBER_OF_EPOCH_BAGS_CR
    long *opsSinceRead;
    
    // for hazard pointer component of this scheme;
    // each thread has a single hazard pointer that it uses to prevent
    // other threads from reclaiming its current scx record before it can
    // clean up after itself.
    AtomicArrayList<T> **announce;         // announce[tid] = pointer to set of hazard pointers for thread tid
    hashset_new<T> **comparing;         // comparing[tid] = set of announced hazard pointers for ALL threads, as collected by thread tid during it's last retire(tid, ...) call

    // number of blocks retired[tid] must contain before it is guaranteed to
    // contain at least 5*numProcesses*MAX_PROTECT_EVEN_IF_QUIESCENT items...
    // why 5*numProcesses*MAX_PROTECT_EVEN_IF_QUIESCENT items?
    // to get amortized constant scanning time per object,
    // the number of elements that retired[tid] must contain
    // before we scan hazard pointers to determine
    // which elements of retired[tid] can be deallocated
    // must be nk+Omega(nk), where
    //      n = number of threads and
    //      k = max number of hazard pointers a thread can hold at once
    // in this context, k=MAX_PROTECT_EVEN_IF_QUIESCENT, since a thread only obtains
    // a hazard pointer to the scx record it has most recently created, and
    // the nodes it points to. so, we just need some constant times
    // numProcesses*MAX_PROTECT_EVEN_IF_QUIESCENT.
    static const int scanThreshold = 4;

    sigset_t neutralizeSignalSet;
    
    inline bool neutralizeOther(const int tid, const int otherTid, const long currentEpoch, const long announceOther) {
#ifdef SEND_CRASH_RECOVERY_SIGNALS
        assert(isQuiescent(tid));
        assert(otherTid != tid);
        // if the epoch bag is too full, then we suspect otherTid has crashed...
        if (epochbags[NUMBER_OF_EPOCH_BAGS_CR*tid+index[tid*PREFETCH_SIZE_WORDS]]->getSizeInBlocks() >= NEUTRALIZE_THRESHOLD_IN_BLOCKS) {
            
            // neutralize otherTid by sending him a signal to make him
            // change what his next step will be, and force him to
            // throw away all pointers into the data structure, and
            // leaveQstate again before re-acquiring any pointers into
            // the data structure. this lets us reclaim memory without
            // waiting for him to progress.
            pthread_t otherPthread = this->recoveryMgr->getPthread(otherTid);
            int error = 0;
//                COUTATOMICTID("sending signal to tid "<<otherTid<<endl);

            if (error = pthread_kill(otherPthread, this->recoveryMgr->neutralizeSignal)) {
                // should never happen
                for (int i=0;i<20;++i) COUTATOMICTID("######################################################"<<endl);
                COUTATOMICTID("error "<<error<<" when trying to pthread_kill(pthread_tFor("<<otherTid<<"), "<<this->recoveryMgr->neutralizeSignal<<")"<<endl);
                assert(isQuiescent(tid));
                const long newann = announcedEpoch[otherTid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
                COUTATOMICTID("otherThread has newann="<<newann<<" with quiescent bit "<<QUIESCENT(newann)<<endl);
                // this can happen because otherTid has terminated.
                // if otherTid is now quiescent, then we can return true...
                if (QUIESCENT(newann)) return true;
                return false;
            } else {
#ifdef AFTER_NEUTRALIZING_WAIT_FOR_QUIESCENCE
                // debug technique: wait until otherTid is
                // either quiescent or has updated its announced epoch.
                for (;;) {
                    TRACE COUTATOMICTID("thread "<<tid<<" waiting for quiescence of thread "<<otherTid<<endl);
                    __sync_synchronize();
                    const long newann = announcedEpoch[otherTid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
                    if (QUIESCENT(newann) || BITS_EPOCH(newann) != BITS_EPOCH(announceOther)) {
                        return true;
                    }
                }
#endif
#ifdef AFTER_NEUTRALIZING_SET_BIT_AND_RETURN_TRUE
                assert(isQuiescent(tid));
                return true;
#endif
            }
        }
        assert(isQuiescent(tid));
#endif
        return false;
    }
    
public:
    
    template<typename _Tp1>
    struct rebind {
        typedef reclaimer_debraplus<_Tp1, Pool> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef reclaimer_debraplus<_Tp1, _Tp2> other;
    };
    
    inline static bool quiescenceIsPerRecordType() { return false; }
    inline static bool supportsCrashRecovery() { return true; }
    inline bool isQuiescent(const int tid) {
        //COUTATOMICTID("IS QUIESCENT EXECUTED"<<endl);
        return QUIESCENT(announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed));
    }
    
    inline static bool isProtected(const int tid, T * const obj) {
        return true;
    }
    
    inline static bool protect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void unprotect(const int tid, T * const obj) {}
    
    inline bool isQProtected(const int tid, T * const obj) {
        return announce[tid]->contains(obj); // this is inefficient, but should only happen when recovering from being neutralized...
    }
    inline bool qProtect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        TRACE COUTATOMICTID("reclaimer_debraplus::protectObjectEvenIfQuiescent(tid="<<tid<</*", "<<*obj<<*/")"<<endl);
        int __size; DEBUG __size = announce[tid]->size();
        DEBUG assert(__size < MAX_PROTECT_EVEN_IF_QUIESCENT);
        announce[tid]->add(obj);
        assert(announce[tid]->contains(obj));
        DEBUG assert(announce[tid]->size() == __size+1);
        // if callbackArg = NULL, we assume notRetiredCallback is a noop.
        if (memoryBarrier) __sync_synchronize(); // prevent retired from being read before we set a hazard pointer to obj, and prevent any future reads of fields of obj from being moved before we announce obj.
        if (notRetiredCallback(callbackArg)) {
            TRACE COUTATOMICTID("notRetiredCallback returns true"<<endl);
            return true;
        } else {
            // obj MAY be retired
            TRACE COUTATOMICTID("notRetiredCallback returns false"<<endl);
            // although we don't care about other threads being able to free
            // this object for efficiency, we still need to null this out
            // because we need to be able to tell whether we successfully
            // protected this when we invoke isProtectedEvenIfQuiescent
            // (otherwise, crash recovery is hard to do)
            announce[tid]->erase(obj); // note: this is inefficient, but it should never happen with regular use.
            DEBUG assert(__size == announce[tid]->size());
            return false;
        }
    }
    inline void qUnprotectAll(const int tid) {
        TRACE COUTATOMICTID("reclaimer_debraplus::unprotectAllObjectsEvenIfQuiescent(tid="<<tid<<")"<<endl);
        assert(isQuiescent(tid));
        announce[tid]->clear();
        assert(announce[tid]->size() == 0);
    }
    
    // rotate the epoch bags and reclaim any objects retired two epochs ago.
    inline void rotateEpochBags(const int tid) {
        assert(isQuiescent(tid));
        // we rotate lists in constant time, and scan hazard pointers
        // when the blockbag from two epochs ago is larger than scanThreshold
        // (using an iterator with erase functionality).
        // maybe in the future we could use bloom filters somehow to determine when no hazard pointer
        // can be present in a block, so we can reclaim the entire block in O(k) time...???
        // (if we're willing to accept k full, unreclaimable blocks per thread, then we can avoid
        //  working with individual elements altogether. we can simply check if each HP is in the bloom
        //  filter for each of c*n blocks (for some constant c), and have some probability of being
        //  able to reclaim (c-1)*n blocks. then, this procedure will be worst case O(n) time.)

        index[tid*PREFETCH_SIZE_WORDS] = (index[tid*PREFETCH_SIZE_WORDS]+1) % NUMBER_OF_EPOCH_BAGS_CR;
        blockbag<T> * const freeable = epochbags[NUMBER_OF_EPOCH_BAGS_CR*tid+index[tid*PREFETCH_SIZE_WORDS]];
        if (freeable->getSizeInBlocks() >= scanThreshold) {

            TRACE COUTATOMICTID("retiring... we have "<<freeable->computeSize()<<" things waiting to be retired in this epoch bag...");
            // hash all announcements
            comparing[tid]->clear();
            assert(comparing[tid]->size() == 0);
            for (int otherTid=0; otherTid < this->NUM_PROCESSES; ++otherTid) {
                int sz = announce[otherTid]->size();
                for (int ixHP = 0; ixHP < sz; ++ixHP) {
                    T* hp = (T*) announce[otherTid]->get(ixHP);
                    if (hp) {
                        int oldSize; DEBUG2 oldSize = comparing[tid]->size();
                        comparing[tid]->insert((T*) hp);
                        DEBUG2 assert(comparing[tid]->size() <= oldSize + 1); // might not increase size if comparing[tid] already contains this item...
                    }
                }
            }
            
            // check if any nodes (from two epochs ago) are announced (qprotected)
            // and swap them to the front of the blockbag.
            // once all announced nodes are at the front of the blockbag,
            // we can free whole blocks in the remainder of the blockbag.
            blockbag_iterator<T> it = freeable->begin();
            blockbag_iterator<T> nextswap = freeable->begin();
            while (it != freeable->end()) {
                if (comparing[tid]->contains(*it)) {
                    // a hazard pointers points to the item
                    it.swap(nextswap.getCurr(), nextswap.getIndex());
                    nextswap++;
                }
                it++;
            }
            block<T> * const curr = nextswap.getCurr();
            if (curr) {
                this->pool->addMoveFullBlocks(tid, freeable, curr);
            }
        }
        
        currentBag[tid*PREFETCH_SIZE_WORDS] = freeable;
        assert(isQuiescent(tid));
    }
    
    // invoke this at the beginning of each operation that accesses
    // objects reclaimed by this epoch manager.
    // returns true if the call rotated the epoch bags for thread tid
    // (and reclaimed any objects retired two epochs ago).
    // otherwise, the call returns false.
    // IMPLIES A FULL MEMORY BARRIER
    inline bool leaveQuiescentState(const int tid, void * const * const reclaimers, const int numReclaimers) {
        SOFTWARE_BARRIER; // prevent any bookkeeping from being moved after this point by the compiler.
        bool result = false;
        long readEpoch = epoch; // multiple of EPOCH_INCREMENT
        assert(!QUIESCENT(readEpoch));
        // if our announced epoch is different from the current epoch
        const long ann = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
        DEBUG2 if (!QUIESCENT(ann)) {
            COUTATOMICTID("NOT QUIESCENT"<<endl);
            exit(-1);
        }
        if (readEpoch != BITS_EPOCH(ann)) {
            // announce the new epoch, and rotate the epoch bags and
            // reclaim any objects retired two epochs ago.
            checked[tid*PREFETCH_SIZE_WORDS] = 0;
            //rotateEpochBags(tid);
            for (int i=0;i<numReclaimers;++i) {
                ((reclaimer_debraplus<T, Pool> * const) reclaimers[i])->rotateEpochBags(tid);
            }
            result = true;
        }
        // note: readEpoch, when written to announcedEpoch[tid],
        //       will set the state to non-quiescent and non-neutralized
        
        // incrementally scan the announced epochs of all threads

        // NUMA friendly version: only check if another thread made progress every MIN_OPS_BEFORE_READ operations
        //                        (reduces added cross-socket cache misses from ~1/operation to ~1/20 operations)
        int otherTid = checked[tid*PREFETCH_SIZE_WORDS];
        if ((++opsSinceRead[tid*PREFETCH_SIZE_WORDS] % MIN_OPS_BEFORE_READ_CR) == 0) {
            long otherAnnounce = announcedEpoch[otherTid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
            if (BITS_EPOCH(otherAnnounce) == readEpoch
                        || QUIESCENT(otherAnnounce)
                        || neutralizeOther(tid, otherTid, readEpoch, otherAnnounce)) {
                const int c = ++checked[tid*PREFETCH_SIZE_WORDS];
                if (c >= this->NUM_PROCESSES) {
                    __sync_bool_compare_and_swap(&epoch, readEpoch, readEpoch+EPOCH_INCREMENT);
                }
            }
        }
        
        /*
        // ORIGINAL DEBRA+ (less NUMA friendly):
        #define MIN_OPS_BEFORE_CAS_EPOCH_CR 100
        int otherTid = checked[tid*PREFETCH_SIZE_WORDS];
        if (otherTid >= this->NUM_PROCESSES) {
            const int c = ++checked[tid*PREFETCH_SIZE_WORDS];
            if (c > MIN_OPS_BEFORE_CAS_EPOCH_CR) {
                __sync_bool_compare_and_swap(&epoch, readEpoch, readEpoch+EPOCH_INCREMENT);
            }
        } else {
            assert(otherTid >= 0);
            long otherAnnounce = announcedEpoch[otherTid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
            if (BITS_EPOCH(otherAnnounce) == readEpoch
                    || QUIESCENT(otherAnnounce)
                    || neutralizeOther(tid, otherTid, readEpoch, otherAnnounce)) {
                const int c = ++checked[tid*PREFETCH_SIZE_WORDS];
                if (c >= this->NUM_PROCESSES && c > MIN_OPS_BEFORE_CAS_EPOCH_CR) {
                    __sync_bool_compare_and_swap(&epoch, readEpoch, readEpoch+EPOCH_INCREMENT);
                }
            }
        }
        */
        
        // it is important that we set the announcedEpoch last, because we must
        // not be neutralized during some of the preceding steps, or we may
        // corrupt the data structure.
        // (on x86/64, writes are not moved earlier in program order, so we don't need any membar before this write.)
        // (on another arch, we'd have to prevent this write from being moved before the write to checked[].)
        assert(isQuiescent(tid));
        SOFTWARE_BARRIER;
        announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(readEpoch, memory_order_relaxed);
        return result;
    }
    // IN A SCHEME THAT SUPPORTS CRASH RECOVERY, THIS IMPLIES A FULL MEMORY BARRIER IFF THIS MOVES THE THREAD FROM AN ACTIVE STATE TO A QUIESCENT STATE
    inline void enterQuiescentState(const int tid) {
        const long ann = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
        announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(GET_WITH_QUIESCENT(ann), memory_order_relaxed);
        assert(isQuiescent(tid));
    }
    
    // for all schemes except reference counting
    inline void retire(const int tid, T* p) {
        assert(isQuiescent(tid));
        currentBag[tid*PREFETCH_SIZE_WORDS]->add(p);
        DEBUG2 this->debug->addRetired(tid, 1);
    }

    void debugPrintStatus(const int tid) {
//        assert(tid >= 0);
//        assert(tid < this->NUM_PROCESSES);
//        long announce = BITS_EPOCH(announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed))/EPOCH_INCREMENT;
//        cout<<"announce="<<announce;
//        cout<<" bags:";
//        for (int i=0;i<NUMBER_OF_EPOCH_BAGS_CR;++i) {
//            cout<<" bag"<<i<<"="<<epochbags[NUMBER_OF_EPOCH_BAGS_CR*tid+i]->computeSize();
//        }
    }

    reclaimer_debraplus(const int numProcesses, Pool *_pool, debugInfo * const _debug, RecoveryMgr<void *> * const _recoveryMgr = NULL)
            : reclaimer_interface<T, Pool>(numProcesses, _pool, _debug, _recoveryMgr) {
        VERBOSE DEBUG COUTATOMIC("constructor reclaimer_debraplus helping="<<this->shouldHelp()<<endl);// scanThreshold="<<scanThreshold<<endl;
        if (_recoveryMgr) COUTATOMIC(" neutralizeSignal="<<this->recoveryMgr->neutralizeSignal<<endl);
        // set up signal set for neutralize signal
        if (sigemptyset(&neutralizeSignalSet)) {
            COUTATOMIC("error creating empty signal set"<<endl);
            exit(-1);
        }
        if (_recoveryMgr) {
            if (sigaddset(&neutralizeSignalSet, this->recoveryMgr->neutralizeSignal)) {
                COUTATOMIC("error adding signal to signal set"<<endl);
                exit(-1);
            }
        }
        
        // all other initialization and allocation
        epoch = 0;
        epochbags = new blockbag<T>*[NUMBER_OF_EPOCH_BAGS_CR*numProcesses];
        currentBag = new blockbag<T>*[numProcesses*PREFETCH_SIZE_WORDS];
        index = new long[numProcesses*PREFETCH_SIZE_WORDS];
        announcedEpoch = new atomic_long[numProcesses*PREFETCH_SIZE_WORDS];
        checked = new long[numProcesses*PREFETCH_SIZE_WORDS];
        announce = new AtomicArrayList<T>*[numProcesses];
        comparing = new hashset_new<T>*[numProcesses];
        opsSinceRead = new long[numProcesses*PREFETCH_SIZE_WORDS];
        for (int tid=0;tid<numProcesses;++tid) {
            for (int i=0;i<NUMBER_OF_EPOCH_BAGS_CR;++i) {
                epochbags[NUMBER_OF_EPOCH_BAGS_CR*tid+i] = new blockbag<T>(this->pool->blockpools[tid]);
            }
            currentBag[tid*PREFETCH_SIZE_WORDS] = epochbags[NUMBER_OF_EPOCH_BAGS_CR*tid];
            index[tid*PREFETCH_SIZE_WORDS] = 0;
            announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(GET_WITH_QUIESCENT(0), memory_order_relaxed);
            checked[tid*PREFETCH_SIZE_WORDS] = 0;
            announce[tid] = new AtomicArrayList<T>(MAX_PROTECT_EVEN_IF_QUIESCENT);
            comparing[tid] = new hashset_new<T>(numProcesses*MAX_PROTECT_EVEN_IF_QUIESCENT);
            opsSinceRead[tid*PREFETCH_SIZE_WORDS] = 0;
        }
    }
    ~reclaimer_debraplus() {
        VERBOSE DEBUG COUTATOMIC("destructor reclaimer_debraplus"<<endl);
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            // move contents of all bags into pool
            for (int i=0;i<NUMBER_OF_EPOCH_BAGS_CR;++i) {
                this->pool->addMoveAll(tid, epochbags[NUMBER_OF_EPOCH_BAGS_CR*tid+i]);
                delete epochbags[NUMBER_OF_EPOCH_BAGS_CR*tid+i];
            }
            delete comparing[tid];
            delete announce[tid];
        }
        delete[] announce;
        delete[] epochbags;
        delete[] index;
        delete[] currentBag;
        delete[] announcedEpoch;
        delete[] checked;
        delete[] comparing;
        delete[] opsSinceRead;
    }

};

#endif

