#ifndef _WEB_SERVER_H__
#define _WEB_SERVER_H__

#include <iostream>
#include "Web.h"
#include "Threadpool.h"
#include "HttpConn.h"
#include "Utils.h"
using namespace std;

/* Maximum number of file descriptors */
static constexpr int MAX_FD = 10000;
/* Maximum number of events */
static constexpr int MAX_EVENT_NUMBER = 10000;
/* Minimum timeout unit */
static constexpr int TIMESLOT = 5;

class WebServer
{
public:
    WebServer();
    ~WebServer();

    /* Initialize the web server */
    void init(int port, string dbUser, string dbPwd, string dbName, 
              int logWrite, int optLinger, int triggerMode, int sqlNum, 
              int threadNum, int closeLog, ActorModel model);
    /* Initialize the thread pool */
    void threadPoolInit();
    /* Initialize the database connection pool */
    void connectionPoolInit();
    /* Initialize the log file */
    void logWriteInit();
    /* Configure the trigger mode */
    void trigMode();
    /* Set up listening */
    void eventListen();
    /* Server main loop */
    void eventLoop();
    /* Initialize the timer */
    void initTimer(int connfd, sockaddr_in clientAddress);
    /* Adjust the timer */
    void adjustTimer(HeapTimer* timer);
    /* Handle timer events */
    void dealTimer(HeapTimer* timer, int sockfd);
    /* Handle client data */
    bool dealClientData();
    /* Handle signal events */
    bool dealWithSignal(bool& timeout, bool& stopServer);
    /* Handle read events */
    void dealWithRead(int sockfd);
    /* Handle write events */
    void dealWithWrite(int sockfd);
    /* Set up daemon process */
    int initDaemon();

public:
    int m_port;
    char* m_root;
    int m_logWrite;
    int m_closeLog;
    ActorModel m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    HttpConn* m_users;

    /* Database related */
    ConnectionPool* m_connPool;
    string m_dbUser;
    string m_dbPassword;
    string m_dbName;
    int m_sqlNum;

    /* Thread pool related */
    Threadpool<HttpConn>* m_pool;
    int m_threadNum;

    /* epoll related */
    epoll_event events[MAX_EVENT_NUMBER];
    int m_listenfd;
    int m_optLinger;
    int m_triggerMode;
    TriggerMode m_lfdMode;
    TriggerMode m_cfdMode;

    /* Timer related */
    ClientData* m_usersTimer;
    Utils m_utils;
};

#endif