#ifndef _CONNECTION_POOL_H__
#define _CONNECTION_POOL_H__

#include <iostream>
#include <cstdio>
#include <mysql.h>
#include <list>
#include "Locker.h"
#include "Log.h"
using namespace std;

/* Singleton class for implementing a database connection pool */
class ConnectionPool
{
public:
    /* Get a database connection from the connection pool */
    MYSQL* getConnection();
    /* Return a database connection to the connection pool */
    bool releaseConnection(MYSQL* conn);
    /* Get the current number of free connections */
    int getFreeConnNumber();
    /* Destroy all connections */
    void destroyPool();
    /* Get the globally unique instance in singleton mode */
    static ConnectionPool* getInstance();
    /* Initialize the database connection pool */
    void initPool(string url, string user, string password, string dbName, int port, int maxConn, int closeLog);

public:
    /* Host address of the database */
    string m_url;
    /* Database port number */
    string m_port;
    /* Database login username */
    string m_user;
    /* Login password for the database */
    string m_password;
    /* Name of the database used */
    string m_dbName;
    /* Log switch */
    int m_closeLog;

private:
    /* Private constructor and destructor */
    ConnectionPool();
    ~ConnectionPool();
    /* Disable object copying */
    ConnectionPool(const ConnectionPool& pool) = delete;
    ConnectionPool& operator=(const ConnectionPool& pool) = delete;

private:
    /* Maximum number of connections */
    int m_maxConn;
    /* Current number of used connections */
    int m_curConn;
    /* Current number of free connections */
    int m_freeConn;
    /* Mutex lock */
    Locker m_lock;
    /* Database connection pool */
    list<MYSQL*> m_connList;
    /* Semaphore */
    Sem m_reserve;
};

/* Automatically manage memory using RAII mechanism */
class ConnectionRAII
{
public:
    /* Take out a MYSQL connection from the connection pool */
    ConnectionRAII(MYSQL** conn, ConnectionPool* connPool);
    /* Automatically release resources */
    ~ConnectionRAII();

private:
    MYSQL* m_connRAII;
    ConnectionPool* m_poolRAII;
};

#endif