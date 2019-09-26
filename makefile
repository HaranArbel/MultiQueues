CC = g++
OBJS = main.o dAryMinHeap.o ParallelDijkstra.o Heap.o MultiQueues.o Allocator.o
EXEC = MultiQueues
COMP_FLAG = -std=c++11
PTHREAD_FLAG = -lpthread

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) $(PTHREAD_FLAG) -o $@ 

main.o: main.cpp dAryMinHeap.h MultiQueues.h ParallelDijkstra.h Graph.h
	$(CC) $(COMP_FLAG) -c $*.cpp 

dAryMinHeap.o: dAryMinHeap.cpp dAryMinHeap.h Heap.h Allocator.h
	$(CC) $(COMP_FLAG) -c $*.cpp

ParallelDijkstra.o: ParallelDijkstra.cpp ParallelDijkstra.h MultiQueues.h Allocator.h Graph.h #pthread/pthread.h
	$(CC) $(COMP_FLAG) -c $*.cpp $(PTHREAD_FLAG)

Heap.o: Heap.cpp Heap.h Graph.h
	$(CC) $(COMP_FLAG) -c $*.cpp

Allocator.o: Allocator.cpp Allocator.h dAryMinHeap.h recordmgr/record_manager.h #pthread/pthread.h#$(RECORDMGR_LIB)/record_manager.h
	$(CC) $(COMP_FLAG) -c $*.cpp $(PTHREAD_FLAG)

MultiQueues.o: MultiQueues.cpp MultiQueues.h dAryMinHeap.h Allocator.h #pthread/pthread.h
	$(CC) $(COMP_FLAG) -c $*.cpp $(PTHREAD_FLAG)

clean:
	rm -f *.o $(EXEC)