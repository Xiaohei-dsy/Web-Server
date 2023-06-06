#ifndef _BLOCK_QUEUE_H__
#define _BLOCK_QUEUE_H__

#include <iostream>
#include <sys/time.h>
#include "Locker.h"
using namespace std;

/*
* Blocking queue implemented by circular array, m_back = (m_back + 1) % m_maxSize;
* Thread safety, a mutex must be added before each operation, and then unlocked after the operation is completed
*/
template<typename T>
class BlockQueue
{
public:
	BlockQueue(int maxSize = 1000);
	~BlockQueue();
	
	/* Clear the queue */
	void clear();
	/* Check if the queue is full */
	bool isFull();
	/* Check if the queue is empty */
	bool isEmpty();
	/* Return the first element of the queue */
	bool front(T& value);
	/* Return the element at the end of the queue */
	bool back(T& value);
	/* Get the current queue size */
	int size();
	/* Get the capacity of the queue */
	int maxSize();
	/* Add an element to the queue */
	bool push(const T& item);
	/* Pop the first element of the queue, if the queue is empty, wait for the condition variable */
	bool pop(T& item);
	/* Added timeout handling */
	bool pop(T& item, int ms_timeout);

private:
	Locker m_mutex;
	Cond m_cond;
	T* m_array;
	int m_size;
	int m_maxSize;
	int m_front;
	int m_back;
};

template<typename T>
BlockQueue<T>::BlockQueue(int maxSize)
{
	if (maxSize <= 0) {
		throw exception();
	}

	m_maxSize = maxSize;
	m_array = new T[maxSize];
	m_size = 0;
	m_front = -1;
	m_back = -1;
}

template<typename T>
BlockQueue<T>::~BlockQueue()
{
	m_mutex.lock();
	if (m_array != nullptr) {
		delete [] m_array;
		m_array = nullptr;
	}
	m_mutex.unlock();
}

template<typename T>
void BlockQueue<T>::clear()
{
	m_mutex.lock();
	m_size = 0;
	m_front = -1;
	m_back = -1;
	m_mutex.unlock();
}

template<typename T>
bool BlockQueue<T>::isFull()
{
	m_mutex.lock();
	if (m_size >= m_maxSize) {
		m_mutex.unlock();
		return true;
	}
	m_mutex.unlock();
	return false;
}

template<typename T>
bool BlockQueue<T>::isEmpty()
{
	m_mutex.lock();
	if (m_size == 0) {
		m_mutex.unlock();
		return true;
	}
	m_mutex.unlock();
	return false;
}

template<typename T>
bool BlockQueue<T>::front(T& value)
{
	m_mutex.lock();
	if (m_size == 0) {
		m_mutex.unlock();
		return false;
	}
	value = m_array[m_front];
	m_mutex.unlock();
	return true;
}

template<typename T>
bool BlockQueue<T>::back(T& value)
{
	m_mutex.lock();
	if (m_size == 0) {
		m_mutex.unlock();
		return false;
	}
	value = m_array[m_back];
	m_mutex.unlock();
	return true;
}

template<typename T>
int BlockQueue<T>::size()
{
	int tmp = 0;
	m_mutex.lock();
	tmp = m_size;
	m_mutex.unlock();
	return tmp;
}

template<typename T>
int BlockQueue<T>::maxSize()
{
	int tmp = 0;
	m_mutex.lock();
	tmp = m_maxSize;
	m_mutex.unlock();
	return tmp;
}

/*
To add elements to the queue, all threads using the queue need to be woken up first
When an element is pushed into the queue, it is equivalent to the producer producing an element
If no thread is currently waiting for the condition variable, it is meaningless to wake up
*/
template<typename T>
bool BlockQueue<T>::push(const T& item)
{
	m_mutex.lock();
	if (m_size >= m_maxSize) {
		m_cond.broadcast();
		m_mutex.unlock();
		return false;
	}

	m_back = (m_back + 1) % m_maxSize;
	m_array[m_back] = item;
	m_size++;
	m_cond.broadcast();
	m_mutex.unlock();
	return true;
}

template<typename T>
bool BlockQueue<T>::pop(T& item)
{
	/* When popping the first element of the queue, if the queue is empty, it will wait for the condition variable */
	m_mutex.lock();
	while (m_size <= 0) {
		if (!m_cond.wait(m_mutex.get())) {
			m_mutex.unlock();
			return false;
		}
	}

	m_front = (m_front + 1) % m_maxSize;
	item = m_array[m_front];
	m_size--;
	m_mutex.unlock();
	return true;
}

template<typename T>
bool BlockQueue<T>::pop(T& item, int ms_timeout)
{
	struct timespec t = { 0, 0 };
	struct timeval now = { 0, 0 };
	gettimeofday(&now, nullptr);

	m_mutex.lock();
	if (m_size <= 0) {
		t.tv_sec = now.tv_sec + ms_timeout / 1000;
		t.tv_nsec = (ms_timeout % 1000) * 1000;
		if (!m_cond.timeWait(m_mutex.get(), t)) {
			m_mutex.unlock();
			return false;
		}
	}

	/* If the queue is still empty when the timeout expires, return after unlocking */
	if (m_size <= 0) {
		m_mutex.unlock();
		return false;
	}
	/* Otherwise, pop the first element */
	m_front = (m_front + 1) % m_maxSize;
	item = m_array[m_front];
	m_size--;
	m_mutex.unlock();
	return true;
}

#endif