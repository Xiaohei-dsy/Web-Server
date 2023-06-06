#include <iostream>
#include "HeapTimer.h"
using namespace std;


HeapTimer::HeapTimer(int delay)
{
	expire = time(nullptr) + delay;
}

TimeHeap::TimeHeap(): capacity(10), curSize(0) 
{
	array = new HeapTimer*[capacity];		// Create heap array
	if (array == nullptr) {
		throw exception();
	}
	for (int i = 0; i < capacity; ++i) {
		array[i] = nullptr;
	}
}

TimeHeap::TimeHeap(int cap): capacity(cap), curSize(0)
{
	array = new HeapTimer*[capacity];		// Create heap array
	if (array == nullptr) {
		throw exception();
	}
	for (int i = 0; i < capacity; ++i) {
		array[i] = nullptr;
	}
}

TimeHeap::TimeHeap(HeapTimer** initArray, int size, int capacity)
: curSize(size), capacity(capacity)
{
	if (capacity < size) {
		throw exception();
	}
	array = new HeapTimer*[capacity];		// Create heap array
	if (array == nullptr) {
		throw exception();
	}
	for (int i = 0; i < capacity; ++i) {
		array[i] = nullptr;
	}
	if (size != 0) {
		/* Initialize the heap array */
		for (int i = 0; i < size; ++i) {
			array[i] = initArray[i];
		}
		/* Build a min heap starting from the last non-leaf node */
		for (int i = (curSize - 1) / 2; i >= 0; --i) {
			percolateDown(i);
		}
	}
}

TimeHeap::~TimeHeap()
{
	for (int i = 0; i < curSize; ++i) {
		if (array[i] != nullptr) {
			delete array[i];
			array[i] = nullptr;
		}
		delete [] array;
	}
}

void TimeHeap::addTimer(HeapTimer* timer)
{
	if (timer == nullptr) {
		return;
	}
	if (curSize >= capacity) {	// If the current heap array capacity is not enough, double it
		resize();
	}
	/* Push the timer to the end of the heap and perform percolate up operation */
	array[curSize] = timer;
	percolateUp(curSize);
	curSize++;
}

void TimeHeap::delTimer(HeapTimer* timer)
{
	if (timer == nullptr) {
		return;
	}
	int loc = timer->loc;
	/* Swap with the timer at the end of the heap */
	swapTimer(loc, curSize - 1);
	curSize--;
	percolateDown(loc);
	delete timer;
}

void TimeHeap::adjustTimer(HeapTimer* timer)
{
	percolateDown(timer->loc);
}

HeapTimer* TimeHeap::top() const
{
	if (empty()) {
		return nullptr;
	}
	return array[0];
}

void TimeHeap::popTimer()
{
	if (empty()) {
		return;
	}
	if (array[0]) {
		delete array[0];
		/* Replace the original heap top element with the last element in the heap array */
		array[0] = array[--curSize];
		/* Perform percolate down operation on the new heap top element to maintain heap properties */
		percolateDown(0);
	}
}

void TimeHeap::tick()
{
	HeapTimer* tmp = array[0];
	/* Process expired timers in the heap */
	time_t cur = time(nullptr);
	while (!empty()) {
		if (tmp == nullptr) {
			break;
		}
		/* If even the top timer in the heap hasn't expired, then the rest haven't either, exit the loop */
		if (tmp->expire > cur) {
			break;
		}
		/* Otherwise, execute the task of the top timer in a loop */
		if (array[0]->cbFunc) {
			array[0]->cbFunc(array[0]->userData);
		}
		/* After executing the task, pop the top element from the heap */
		popTimer();
		tmp = array[0];
	}
}

bool TimeHeap::empty() const
{
	return curSize == 0;
}

void TimeHeap::percolateDown(int hole)
{
	int left = 2 * hole + 1, right = 2 * hole + 2;
	int minLoc = hole;
	if (left < curSize && array[left]->expire < array[minLoc]->expire) {
		minLoc = left;
	}
	if (right < curSize && array[right]->expire < array[minLoc]->expire) {
		minLoc = right;
	}
	if (minLoc != hole) {
		swapTimer(minLoc, hole);
		percolateDown(minLoc);
	}
}

void TimeHeap::percolateUp(int hole)
{
	HeapTimer* timer = array[hole];

	int parent = 0;
	for (; hole > 0; hole = parent) {
		parent = (hole - 1) / 2;
		if (array[parent]->expire <= timer->expire) {
			break;
		}
		array[hole] = array[parent];
		array[hole]->loc = hole;
	}
	array[hole] = timer;
	array[hole]->loc = hole;
}

void TimeHeap::resize()
{
	HeapTimer** temp = new HeapTimer*[2 * capacity];
	if (temp == nullptr) {
		throw exception();
	}
	for (int i = 0; i < 2 * capacity; ++i) {
		temp[i] = nullptr;
	}
	capacity = 2 * capacity;
	for (int i = 0; i < curSize; ++i) {
		temp[i] = array[i];
	}
	delete [] array;
	array = temp;
}

void TimeHeap::swapTimer(int a, int b)
{
	HeapTimer* tmp = array[a];
	array[a] = array[b];
	array[a]->loc = a;		// Update the position of the timer

	array[b] = tmp;
	array[b]->loc = b;		// Update the position of the timer
}

void TimeHeap::printSelf()
{
	printTree(0);
}

void TimeHeap::printTree(int hole)
{
	static int level = -1; // Records the level of the tree
	int i;

	if (hole >= curSize) {
		return;
	}

	level++;
	printTree(2 * hole + 2);
	level--;

	level++;
	for (i = 0; i < level; i++) {
		printf("\t");
	}
	printf("%2d\n", array[hole]->expire);
	printTree(2 * hole + 1);
	level--;
}