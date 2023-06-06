#ifndef _TIMER_LIST__
#define _TIMER_LIST__

#include <iostream>
#include "Web.h"
#include "Log.h"
using namespace std;

/* Forward declaration */
class UtilTimer;
/* User data structure */
struct ClientData
{
    /* Client address */
    sockaddr_in address;
    /* Socket file descriptor */
    int sockfd;
    /* Timer */
    UtilTimer* timer;
};

class UtilTimer
{
public:
    UtilTimer(): prev(nullptr), next(nullptr) {}

public:
    /* Expiration time of the task, using absolute time */
    time_t expire;
    /* Callback function of the task */
    void (*cbFunc)(ClientData*);
    /* User data processed by the callback function */
    ClientData* userData;
    /* Pointer to the previous timer */
    UtilTimer* prev;
    /* Pointer to the next timer */
    UtilTimer* next;
};

/* Doubly linked list of timers sorted in ascending order */
class SortTimerList
{
public:
    SortTimerList();
    ~SortTimerList();

    /* Add the target timer to the list */
    void addTimer(UtilTimer* timer);
    /* Adjust the position of the corresponding timer in the list */
    void adjustTimer(UtilTimer* timer);
    /* Remove the target timer from the list */
    void deleteTimer(UtilTimer* timer);
    /* Process the expired tasks on the list */
    void tick();

private:
    /* An overloaded auxiliary function */
    void addTimer(UtilTimer* timer, UtilTimer* listHead);

private:
    UtilTimer* m_head;
    UtilTimer* m_tail;
};

#endif