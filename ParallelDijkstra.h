//
// Created by Haran Arbel on 2019-04-21.
//

#ifndef MULTIQUEUE_DIJKSTRA_HPP
#define MULTIQUEUE_DIJKSTRA_HPP

#include "Graph.h"
#include "MultiQueues.h"
#include "Allocator.h" //todo edit includes all project

void dijkstra_shortest_path(Graph *G, int c, int p);
void *parallel_Dijkstra(void *void_input);

#endif //MULTIQUEUE_DIJKSTRA_HPP
