#ifndef _LOCKER_H__
#define _LOCKER_H__

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/* Class encapsulating a semaphore */
class Sem
{
public:
    /* Create and initialize a semaphore */
    Sem();
    /* Create and initialize a semaphore with an initial value of num */
    Sem(int num);
    /* Destroy the semaphore */
    ~Sem();
    /* Wait for a semaphore */
    bool wait();
    /* Increase the value of the semaphore */
    bool post();

private:
    sem_t m_sem;
};

/* Class encapsulating a mutex lock */
class Locker
{
public:
    /* Create and initialize a mutex lock */	
    Locker();
    /* Destroy the mutex lock */
    ~Locker();
    /* Acquire the mutex lock */
    bool lock();
    /* Release the mutex lock */
    bool unlock();
    /* Get the current mutex lock object */
    pthread_mutex_t* get();

private:
    pthread_mutex_t m_mutex;
};

/* Class encapsulating a condition variable */
class Cond
{
public:
    /* Create and initialize a condition variable */
    Cond();
    /* Destroy the condition variable */
    ~Cond();

    /* Wait for the condition variable */
    bool wait(pthread_mutex_t* mutex);
    bool timeWait(pthread_mutex_t* mutex, struct timespec t);

    /* Wake up the waiting queue of the condition variable */
    bool signal();
    bool broadcast();

private:
    // pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif