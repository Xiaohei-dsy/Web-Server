#ifndef _THREADPOOL_H__
#define _THREADPOOL_H__

#include <list>
#include <exception>
#include <pthread.h>
#include "Web.h"
#include "Locker.h"
#include "ConnectionPool.h"

/* Thread pool class, defined as a template class for code reuse, where T is the task class */
template<typename T>
class Threadpool
{
public:
    Threadpool(ActorModel model, ConnectionPool* connPool, int threadNumber = 8, int maxRequests = 10000);
    ~Threadpool();
    /* Add a task to the request queue */
    bool append(T* request, int state);
    /* Add a task in proactor mode */
    bool append_p(T* request);

private:
    /* Function executed by the worker threads, continuously retrieves tasks from the work queue and executes them */
    static void* worker(void* arg);
    void run();

private:
    /* Number of threads in the thread pool */
    int m_threadNumber;
    /* Maximum number of requests allowed in the request queue */
    int m_maxRequests;
    /* Array describing the thread pool, with a size of m_threadNumber */
    pthread_t* m_threads;
    /* Request queue */
    std::list<T*> m_workQueue;
    /* Mutex lock to protect the request queue */
    Locker m_queueLocker;
    /* Semaphore to indicate whether there are tasks to be processed */
    Sem m_queueStat;
    /* Flag to stop the threads */
    bool m_stop;
    /* Database connection pool */
    ConnectionPool* m_connPool;
    /* Model switch */
    ActorModel m_model;
};

template<typename T>
Threadpool<T>::Threadpool(ActorModel model, ConnectionPool* connPool, int threadNumber, int maxRequests)
{
    /* Parameter validation */
    if (threadNumber <= 0 || maxRequests <= 0) {
        throw std::exception();
    }
    /* Member variable initialization */
    m_threadNumber = threadNumber;
    m_maxRequests = maxRequests;
    m_stop = false;
    m_threads = new pthread_t[m_threadNumber];
    m_model = model;
    m_connPool = connPool;
    if (m_threads == nullptr) {
        throw std::exception();
    }

    /* Create threadNumber threads and set them as detached */
    for (int i = 0; i < threadNumber; ++i) {
        printf("create the %dth thread\n", i);
        if (pthread_create(m_threads + i, nullptr, worker, this) != 0) {
            delete [] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
Threadpool<T>::~Threadpool()
{
    delete [] m_threads;
    m_stop = true;
}

template<typename T>
bool Threadpool<T>::append(T* request, int state)
{
    m_queueLocker.lock();
    if (m_workQueue.size() >= m_maxRequests) {
        m_queueLocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workQueue.push_back(request);
    m_queueLocker.unlock();
    m_queueStat.post();
    return true;
}

template<typename T>
bool Threadpool<T>::append_p(T* request)
{
    /* When operating on the work queue, be sure to lock it, as it is shared by all threads */
    m_queueLocker.lock();
    if (m_workQueue.size() >= m_maxRequests) {
        m_queueLocker.unlock();
        return false;
    }
    m_workQueue.push_back(request);
    m_queueLocker.unlock();
    m_queueStat.post();
    return true;
}

template<typename T>
void* Threadpool<T>::worker(void* arg)
{
    Threadpool* pool = (Threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void Threadpool<T>::run()
{
    while (!m_stop) {
        m_queueStat.wait();
        m_queueLocker.lock();
        if (m_workQueue.empty()) {
            m_queueLocker.unlock();
            continue;
        }
        T* request = m_workQueue.front();
        m_workQueue.pop_front();
        m_queueLocker.unlock();
        if (!request) {
            continue;
        }
        if (m_model == REACTOR) {
            if (request->m_state == 0) {
                if (request->readn()) {
                    request->m_improv = 1;
                    ConnectionRAII mysqlConn(&request->m_mysql, m_connPool);
                    request->process();
                }
                else {
                    request->m_improv = 1;
                    request->m_timerFlag = 1;
                }
            }
            else {
                if (request->writen()) {
                    request->m_improv = 1;
                }
                else {
                    request->m_improv = 1;
                    request->m_timerFlag = 1;
                }
            }
        }
        else {
            ConnectionRAII mysqlConn(&request->m_mysql, m_connPool);
            request->process();
        }
    }
}

#endif