#include <iostream>
#include "WebServer.h"
using namespace std;

WebServer::WebServer()
{
    /* Initialize HttpConn objects */
    m_users = new HttpConn[MAX_FD];

    /* root folder path */
    char serverPath[200] = { 0 };
    getcwd(serverPath, 200);
    char root[6] = "/root";
    m_root = (char*)malloc(strlen(serverPath) + strlen(root) + 1);
    strcpy(m_root, serverPath);
    strcat(m_root, root);

    /* Initialize timers */
    m_usersTimer = new ClientData[MAX_FD];
}

WebServer::~WebServer()
{
    free(m_root);
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] m_users;
    delete[] m_usersTimer;
    delete m_pool;
}

void WebServer::init(int port, string dbUser, string dbPwd, string dbName,
                     int logWrite, int optLinger, int triggerMode, int sqlNum,
                     int threadNum, int closeLog, ActorModel model)
{
    m_port = port;
    m_dbUser = dbUser;
    m_dbPassword = dbPwd;
    m_dbName = dbName;
    m_sqlNum = sqlNum;
    m_threadNum = threadNum;
    m_logWrite = logWrite;
    m_optLinger = optLinger;
    m_triggerMode = triggerMode;
    m_closeLog = closeLog;
    m_actormodel = model;
}

void WebServer::threadPoolInit()
{
    m_pool = new Threadpool<HttpConn>(m_actormodel, m_connPool, m_threadNum);
}

void WebServer::connectionPoolInit()
{
    /* Initialize the database connection pool */
    m_connPool = ConnectionPool::getInstance();
    m_connPool->initPool("localhost", m_dbUser, m_dbPassword, m_dbName, 3306, m_sqlNum, m_closeLog);
    /* Preload the contents of the database into memory */
    m_users->initMysqlResult(m_connPool);
}

void WebServer::logWriteInit()
{
    if (m_closeLog == 0) {
        if (m_logWrite == 1) {
            Log::getInstance()->init("./ServerLog", m_closeLog, 2000, 800000, 800);
        }
        else {
            Log::getInstance()->init("./ServerLog", m_closeLog, 2000, 800000, 0);
        }
    }
}

void WebServer::trigMode()
{
    if (m_triggerMode == 0) {
        m_lfdMode = EPOLL_LT;
        m_cfdMode = EPOLL_LT;
    }
    else if (m_triggerMode == 1) {
        m_lfdMode = EPOLL_LT;
        m_cfdMode = EPOLL_ET;
    }
    else if (m_triggerMode == 2) {
        m_lfdMode = EPOLL_ET;
        m_cfdMode = EPOLL_LT;
    }
    else if (m_triggerMode == 3) {
        m_lfdMode = EPOLL_ET;
        m_cfdMode = EPOLL_ET;
    }
}

void WebServer::eventListen()
{
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    /* Graceful close of the connection */
    if (m_optLinger == 0) {
        struct linger tmp = { 0, 1 };
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (m_optLinger == 1) {
        struct linger tmp = { 1, 1 };
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    /* The following two lines are for avoiding TIME_WAIT state, only for debugging, should be removed in actual use */
    int reuse = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    /* Prepare address structure */
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(m_port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind address structure */
    int ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    /* Set listening */
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    m_utils.init(TIMESLOT);

    /* Create epoll kernel event table */
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    /* Register listening file descriptor to epoll kernel event table */
    m_utils.addfd(m_epollfd, m_listenfd, false, m_lfdMode);
    HttpConn::m_epollfd = m_epollfd;

    /* Create a pipe for notifying the timer and signal events (unified event source) */
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    m_utils.setNonblocking(m_pipefd[1]);
    m_utils.addfd(m_epollfd, m_pipefd[0], false, EPOLL_LT);

    m_utils.addSig(SIGPIPE, SIG_IGN);
    m_utils.addSig(SIGALRM, m_utils.sigHandler, false);
    m_utils.addSig(SIGTERM, m_utils.sigHandler, false);

    alarm(TIMESLOT);
    Utils::u_epollfd = m_epollfd;
    Utils::u_pipefd = m_pipefd;
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stopServer = false;
    while (!stopServer)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;

            /* Handle new client connections */
            if (sockfd == m_listenfd) {
                bool flag = dealClientData();
                if (flag == false) {
                    LOG_ERROR("%s", "dealWithClientData failure");
                    continue;
                }
            }
            /* Handle exceptional events */
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                /* Server closes the connection, remove the corresponding timer */
                HeapTimer* timer = m_usersTimer[sockfd].timer;
                dealTimer(timer, sockfd);
            }
            /* Handle signal events */
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
                bool flag = dealWithSignal(timeout, stopServer);
                if (flag == false) {
                    LOG_ERROR("%s", "dealWithSignal failure");
                }
            }
            /* Handle read events on connection file descriptors */
            else if (events[i].events & EPOLLIN) {
                dealWithRead(sockfd);
            }
            /* Handle write events on connection file descriptors */
            else if (events[i].events & EPOLLOUT) {
                dealWithWrite(sockfd);
            }
        }
        if (timeout) {
            m_utils.timerHandler();
            timeout = false;
        }
    }
}

void WebServer::initTimer(int connfd, sockaddr_in clientAddress)
{
    m_users[connfd].init(connfd, clientAddress, m_root, m_cfdMode, m_closeLog, m_dbUser, m_dbPassword, m_dbName);
    /* Initialize clientData */
    /* Create a timer, set callback function and timeout, bind user data, and add the timer to the list */
    m_usersTimer[connfd].address = clientAddress;
    m_usersTimer[connfd].sockfd = connfd;
    HeapTimer* timer = new HeapTimer(3 * TIMESLOT);
    timer->userData = &m_usersTimer[connfd];
    timer->cbFunc = cbFunc;
    m_usersTimer[connfd].timer = timer;
    m_utils.m_timeHeap.addTimer(timer);
}

void WebServer::adjustTimer(HeapTimer* timer)
{
    time_t cur = time(nullptr);
    timer->expire = cur + 3 * TIMESLOT;
    m_utils.m_timeHeap.adjustTimer(timer);
    LOG_INFO("%s", "adjust timer once");
}

void WebServer::dealTimer(HeapTimer* timer, int sockfd)
{
    timer->cbFunc(&m_usersTimer[sockfd]);
    if (timer) {
        m_utils.m_timeHeap.delTimer(timer);
    }
    LOG_INFO("close fd %d", m_usersTimer[sockfd].sockfd);
}

bool WebServer::dealClientData()
{
    struct sockaddr_in clientAddress;
    socklen_t clientAddrlen = sizeof(clientAddress);
    /* Level-triggered mode */
    if (m_lfdMode == EPOLL_LT) {
        int connfd = accept(m_listenfd, (struct sockaddr*)&clientAddress, &clientAddrlen);
        if (connfd < 0) {
            LOG_ERROR("%s: errno is %d", "accept error", errno);
            return false;
        }
        if (HttpConn::m_userCount >= MAX_FD) {
            m_utils.showError(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        initTimer(connfd, clientAddress);
    }
    /* Edge-triggered mode, accept all connections at once */
    else {
        while (true) {
            int connfd = accept(m_listenfd, (struct sockaddr*)&clientAddress, &clientAddrlen);
            if (connfd < 0) {
                if (errno != EWOULDBLOCK) {
                    LOG_ERROR("%s: errno is %d", "accept error", errno);
                }
                break;
            }
            if (HttpConn::m_userCount >= MAX_FD) {
                m_utils.showError(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            initTimer(connfd, clientAddress);
        }
        return false;
    }
    return true;
}

bool WebServer::dealWithSignal(bool& timeout, bool& stopServer)
{
    int ret = 0;
    char signals[1024];
    /* Receive signals from the pipe */
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1 || ret == 0) {
        return false;
    }
    for (int i = 0; i < ret; ++i) {
        switch (signals[i])
        {
        case SIGALRM:
            timeout = true;
            break;
        case SIGTERM:
            stopServer = true;
            break;
        default:
            break;
        }
    }
    return true;
}

void WebServer::dealWithRead(int sockfd)
{
    HeapTimer* timer = m_usersTimer[sockfd].timer;
    /* In the reactor model, the main thread only needs to accept new connections, and read/write operations are handled by the worker threads */
    if (m_actormodel == REACTOR) {
        /* Update the timer's timeout */
        if (timer) {
            adjustTimer(timer);
        }
        m_pool->append(m_users + sockfd, 0);

        while (true) {
            if (m_users[sockfd].m_improv == 1) {
                if (m_users[sockfd].m_timerFlag == 1) {
                    dealTimer(timer, sockfd);
                    m_users[sockfd].m_timerFlag = 0;
                }
                m_users[sockfd].m_improv = 0;
                break;
            }
        }
    }
    /* In the proactor model, the main thread handles I/O operations, and the worker threads handle business logic */
    else {
        if (m_users[sockfd].readn()) {
            LOG_INFO("deal with the client(%s)", inet_ntoa(m_users[sockfd].getAddress()->sin_addr));
            m_pool->append_p(m_users + sockfd);
            if (timer) {
                adjustTimer(timer);
            }
        }
        else {
            dealTimer(timer, sockfd);
        }
    }
}

void WebServer::dealWithWrite(int sockfd)
{
    HeapTimer* timer = m_usersTimer[sockfd].timer;
    if (m_actormodel == REACTOR) {
        if (timer) {
            adjustTimer(timer);
        }
        m_pool->append(m_users + sockfd, 1);
        while (true) {
            if (m_users[sockfd].m_improv == 1) {
                if (m_users[sockfd].m_timerFlag == 1) {
                    dealTimer(timer, sockfd);
                    m_users[sockfd].m_timerFlag = 0;
                }
                m_users[sockfd].m_improv = 0;
                break;
            }
        }
    }
    else {
        if (m_users[sockfd].writen()) {
            LOG_INFO("send data to the client(%s)", inet_ntoa(m_users[sockfd].getAddress()->sin_addr));
            if (timer) {
                adjustTimer(timer);
            }
        }
        else {
            dealTimer(timer, sockfd);
        }
    }
}

int WebServer::initDaemon()
{
    /* Ignore terminal I/O signals and STOP signals */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGSTOP, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    int pid = fork();
    if (pid > 0) {
        /* Terminate the parent process to make the child process a background process */
        exit(0);
    }
    else if (pid < 0) {
        return -1;
    }
    /* Establish a new process group, making the child process the leader of this group */
    setsid();
    /* Create another child process, exit the parent process, ensuring that this process is not the process group leader */
    pid = fork();
    if (pid > 0) {
        exit(0);
    }
    else if (pid < 0) {
        return -1;
    }
    /* Close all file descriptors inherited from the parent process that are no longer needed */
    for (int i = 0; i < NOFILE; ++i) {
        close(i);
    }
    /* Set the file creation mask to 0 */
    umask(0);
    /* Ignore SIGCHLD signals */
    signal(SIGCHLD, SIG_IGN);

    return 0;
}