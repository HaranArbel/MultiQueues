//
// Created by Haran Arbel on 2019-08-28.
//

#include "Allocator.h"

record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,Offer> * Allocator::mgr;

std::mutex Allocator::mgr_lock;

