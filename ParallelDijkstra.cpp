//
// Created by Haran Arbel on 2019-04-21.
//

#include <stdbool.h>
#include <iostream>
#include <fstream>
#include <limits>
#include <pthread.h>
#include "ParallelDijkstra.h"
#include "MultiQueues.h"
#include "Allocator.h"
#include <unistd.h>
#include <array>

using namespace std;

pthread_mutex_t done_work_lock;
pthread_cond_t done_work_cond;


bool finished_work(bool done[], int numOfThreads){
    for (int i = 0; i < numOfThreads; i++){
        if(!done[i]){
            return false;
        }
    }
    return true;
}

void relax(MultiQueues* queue, int* distances, std::mutex **distancesLocks, std::mutex **offersLocks, Offer **offers, Vertex* vertex, int alt, int tid) {
    Offer* curr_offer;

    offersLocks[vertex->index]->lock();

    distancesLocks[vertex->index]->lock();
    int curr_dist = distances[vertex->index];
    distancesLocks[vertex->index]->unlock();

    if (alt < curr_dist) {

        curr_offer = offers[vertex->index];
        if (curr_offer == NULL || alt < curr_offer->dist ){

            Offer* new_offer = queue->insert(vertex, alt, tid);
            new_offer->vertex = vertex;
            new_offer->dist = alt;

            Allocator::enterQuiescentState(tid);
            Offer* offer = Allocator::allocate(tid);
            offer->vertex = vertex;
            offer->dist = alt;
            Allocator::leaveQuiescentState(tid);


            if (offers[vertex->index]) {
                Allocator::enterQuiescentState(tid);
                Allocator::free(offers[vertex->index], tid);
                Allocator::leaveQuiescentState(tid);

            }
            offers[vertex->index] = offer;

        }
    }
    offersLocks[vertex->index]->unlock();

}


class ThreadInput {
public:
    bool *done;
    MultiQueues* queue;
    int p;
    Graph *G;
    std::mutex **offersLocks;
    std::mutex **distancesLocks;
    int tid;
    int * distances;
    Offer ** offers;

    ThreadInput(bool *done, MultiQueues *queue, int p, Graph *G, int * distances, std::mutex **offersLocks,
                std::mutex **distancesLocks, Offer ** offers, int tid) {
        this->done = done;
        this->queue = queue;
        this->p = p;
        this->G = G;
        this->distances = distances;
        this->offersLocks = offersLocks;
        this->distancesLocks = distancesLocks;
        this->offers = offers;
        this->tid = tid;
    }
};


void *parallel_Dijkstra(void *void_input) {

    ThreadInput * input = (ThreadInput *) void_input;
    bool * done = input->done;
    MultiQueues *queue = input->queue;
    Graph *G = input->G;
    Offer **offers = input->offers;
    int tid = input->tid;

    Vertex *curr_v;
    Vertex *neighbor;
    bool explore = true;
    int curr_dist = -1;
    int alt;
    int weight;

    int* distances = input->distances;
    std::mutex ** offersLocks = input->offersLocks;
    std::mutex **distancesLocks = input->distancesLocks;
    int p =input->p;

    Offer min_offer = {};
    bool got_min;


    while (true) {
        if (queue->is_empty()== true){
            done[tid] = true;
        }

        if (!done[tid]){
            got_min = queue->deleteMin(&min_offer, tid);
        }
        else{
            if (finished_work(done, p))
                return NULL;
            else
                continue;
        }
        if (!got_min) {
            done[tid] = true;
            if (finished_work(done, p))
                return NULL;
            else
                continue;
        }

        done[tid] = false;

        curr_v = min_offer.vertex;
        curr_dist = min_offer.dist;

        distancesLocks[curr_v->index]->lock();
        if (curr_dist < distances[curr_v->index]) {
            distances[curr_v->index] = curr_dist;
            explore = true;
        } else {
            explore = false;
        }
        distancesLocks[curr_v->index]->unlock();

        if (explore) {
            for (int i = 0; i < (curr_v->neighbors.size()); i++) {
                neighbor = curr_v->neighbors[i].first;
                weight = curr_v->neighbors[i].second;
                alt = curr_dist + weight;
                relax(queue,distances, distancesLocks,offersLocks,offers,neighbor,alt, tid);
            }
        }

    }
}




void dijkstra_shortest_path(Graph *G, int c, int p) {

    Allocator a = Allocator();
    Allocator::init_allocator(p);

    pthread_mutex_init(&done_work_lock, NULL);
    pthread_cond_init(&done_work_cond, NULL);

    // create priority queue
    MultiQueues *queue = new MultiQueues(c,p);
    Offer min_offer = {};


    int distances[G->vertices.size()];
    Offer *offers[G->vertices.size()];

    std::mutex **offersLocks = new std::mutex *[G->vertices.size()];
    std::mutex **distancesLocks = new std::mutex *[G->vertices.size()];

    //init
    for (int i = 0; i < G->vertices.size(); i++) {
        distances[i] = INT_MAX;
        offers[i] =NULL;
    }

    //init locks
    for (int i = 0; i < G->vertices.size(); i++) {
        offersLocks[i] = new std::mutex();
        distancesLocks[i] = new std::mutex();
    }

    // initialization
    distances[G->source] = INT_MAX;

    //don't need to use Debra by Or
    min_offer.vertex = G->vertices[G->source];
    min_offer.dist = 0;

    queue->insert(min_offer.vertex, min_offer.dist, 0);

    int num_of_threads = p;
    pthread_t threads[num_of_threads];
    bool done[p];

    std::vector<ThreadInput*>to_delete;
    for (int i = 0; i < num_of_threads; i++) {
        done[i] = false;
        to_delete.push_back(new ThreadInput(done, queue, p, G, distances, offersLocks, distancesLocks, offers, i));

        pthread_create(&threads[i], NULL, &parallel_Dijkstra, (void *) to_delete[i]);

    }

    for (long i = 0; i < num_of_threads; i++) {
        (void) pthread_join(threads[i], NULL);
    }

    for (auto p : to_delete){
        delete p;
    }
    to_delete.clear();

    ofstream myFile;
    myFile.open ("output.txt");
    for (int i = 0; i < G->vertices.size(); i++) {
        myFile << distances[i] << std::endl;
    }
    myFile.close();

    for(int i=0; i<G->vertices.size(); i++){
        delete offersLocks[i];
        delete distancesLocks[i];
        if (offers[i]){
//            delete offers[i];
            Allocator::enterQuiescentState(0);
            Allocator::free(offers[i], 0);
            Allocator::leaveQuiescentState(0);
        }
    }
    delete[] offersLocks;
    delete[] distancesLocks;

    delete queue;


}


