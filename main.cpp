#include <iostream>
#include "dAryMinHeap.h"
#include "MultiQueues.h"
#include <thread>
#include <unistd.h>
#include <fstream>
#include <string>
#include <stdlib.h>
#include "Graph.h"
#include "ParallelDijkstra.h"

using namespace std;
#define NUM_OF_THREADS 80

int main(int argc,  char *argv[]) {

    string pathToFile = argv[1];
    ifstream f;
    f.open (pathToFile);
    if (!f) {
        cerr << "Unable to open file " + pathToFile;
        exit(1);
    }

    int tuning_parameter = atoi(argv[2]);
    if (!tuning_parameter) {
        cerr << "A tuning parameter must be provided";
        exit(1);
    }

    Graph *G = new Graph();

    string line;
    string firstLine;
    int source_index;
    char* n;

    getline(f,firstLine); //get the first line of # nodes, # edges and source node

    // get source vertex
    char str [line.size() + 1];
    strncpy(str, firstLine.c_str(), firstLine.size() + 1);
    char *token = strtok(str, " ");
    int num_vertices = strtol(token, &n, 0);
    token = strtok(NULL, " ");
    int num_edges = strtol(token, &n, 0);
    token = strtok(NULL, " ");
    source_index = strtol(token, &n, 0);
    G->source = source_index;

    G->vertices.resize(num_vertices);
    for (int i = 0; i < num_vertices; i++) {
        G->vertices[i] = new Vertex();
    }

    while (getline(f,line)) {
        Vertex *source = nullptr;
        Vertex *v1 = nullptr;
        Vertex *v2 = nullptr;
        int v1_index;
        int v2_index;
        int weight;
        char *p;
        char *q;
        char *m;

        // copy line
        char str[line.size() + 1];
        strncpy(str, line.c_str(), line.size() + 1);

        // Returns first token
        token = strtok(str, " ");

        // get vertex v1 index
        v1_index = strtol(token, &p, 0);

        // get vertex v2 index
        v2_index = strtol(strtok(NULL, " "), &q, 0);

        v1 = G->vertices[v1_index];
        v2 = G->vertices[v2_index];
        // get edge weight
        weight = strtol(strtok(NULL, " "), &m, 0);

        v1->index = v1_index;
        v2->index = v2_index;
        //Weight

        v1->neighbors.push_back(make_pair(v2, weight));
        v2->neighbors.push_back(make_pair(v1, weight));

    }

    f.close();
    dijkstra_shortest_path(G, tuning_parameter, NUM_OF_THREADS);
    delete G;

}