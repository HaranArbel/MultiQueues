//
// Created by Haran Arbel on 2019-04-21.
//

#ifndef MULTIQUEUE_GRAPH_H
#define MULTIQUEUE_GRAPH_H


#include <iostream>
#include <list>
#include <vector>
#include <mutex>


using namespace std;

struct Vertex {
    unsigned int index;
    vector < pair <Vertex*, int> > neighbors; // Vertex + weight of edge that connect vertices
};


class Graph {
    public:
        ~Graph(){
            for(int i=0; i<vertices.size(); i++){
                if(vertices[i]){
                    delete(vertices[i]);
                }
            }
        }
        int source;
        vector <Vertex*> vertices;

};

#endif //MULTIQUEUE_GRAPH_H
