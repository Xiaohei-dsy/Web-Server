#ifndef _UTILS_H__
#define _UTILS_H__

#include <iostream>
#include "HeapTimer.h"
#include "Web.h"
using namespace std;

class Utils
{
public:
    Utils();
    ~Utils();

    void init(int timeslot);
    /* Set a file descriptor to non-blocking mode */
    int setNonblocking(int fd);
    /* Register an event in the epoll kernel event table, using ET mode and optionally enabling EPOLLONESHOT */
    void addfd(int epollfd, int fd, bool oneShot, TriggerMode mode);
    /* Signal handler callback function */
    static void sigHandler(int sig);
    /* Register a signal handler function */
    void addSig(int sig, void(handler)(int), bool restart = true);
    /* Timed task handler, re-register the timer to continuously trigger the SIGALARM signal */
    void timerHandler();
    /* Send error message to the client connection */
    void showError(int connfd, const char* info);

public:
    static int* u_pipefd;
    TimeHeap m_timeHeap;
    static int u_epollfd;
    int m_timeslot;
};

void cbFunc(ClientData* userData);

#endif