#pragma once
#include <iostream>
#include <unordered_map>
#include <netinet/in.h>
#include <time.h>
using namespace std;

#define BUFFER_SIZE	64

class HeapTimer;	// Forward declaration
/* Bind socket and timer */
struct ClientData
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    HeapTimer* timer;
};

/* Timer class */
class HeapTimer
{
public:
    HeapTimer(int delay);

public:
    time_t expire;					/* The absolute time when the timer will expire */
    void (*cbFunc)(ClientData*);	/* Callback function of the timer */
    ClientData* userData;			/* User data */
    int loc;						/* The position of the timer in the heap */
};

/* Time heap class */
class TimeHeap
{
public:
    TimeHeap();
    /* Initialize an empty heap with capacity cap */
    TimeHeap(int cap);
    /* Initialize the heap with an existing array */
    TimeHeap(HeapTimer** initArray, int size, int capacity);
    /* Destroy the time heap */
    ~TimeHeap();
    /* Add a target timer */
    void addTimer(HeapTimer* timer);
    /* Delete a target timer */
    void delTimer(HeapTimer* timer);
    /* Adjust the position of a target timer */
    void adjustTimer(HeapTimer* timer);
    /* Get the top timer in the heap */
    HeapTimer* top() const;
    /* Delete the top timer in the heap */
    void popTimer();
    /* Heartbeat function */
    void tick();
    /* Check if the heap is empty */
    bool empty() const;
    /* For testing purposes */
    void printSelf();

private:
    /* Percolate down in the min heap, ensuring that the subtree rooted at the holeth node in the heap array has the min heap property */
    void percolateDown(int hole);
    /* Percolate up */
    void percolateUp(int hole);
    /* Double the capacity of the heap array */
    void resize();
    /* Swap the positions of two elements in the heap */
    void swapTimer(int a, int b);
    /* For testing purposes */
    void printTree(int hole);

private:
    HeapTimer** array;						// Heap array
    int capacity;							// Capacity of the heap array
    int curSize;							// Current number of elements in the heap array
};