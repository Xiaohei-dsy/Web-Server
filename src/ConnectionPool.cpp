#include <iostream>
#include "ConnectionPool.h"
using namespace std;

MYSQL* ConnectionPool::getConnection()
{
	MYSQL* conn = nullptr;
	if (m_connList.empty()) {
		return nullptr;
	}
	m_reserve.wait();
	m_lock.lock();
	conn = m_connList.front();
	m_connList.pop_front();

	
    /* Update the current number of connections and the number of idle connections */
	m_freeConn--;
	m_curConn++;
	m_lock.unlock();
	return conn;
}

bool ConnectionPool::releaseConnection(MYSQL* conn)
{
	if (conn == nullptr) {
		return false;
	}
	m_lock.lock();
	m_connList.push_back(conn);
	m_freeConn++;
	m_curConn--;
	m_lock.unlock();
	m_reserve.post();
	return true;
}

int ConnectionPool::getFreeConnNumber()
{
	return this->m_freeConn;
}

void ConnectionPool::destroyPool()
{
	m_lock.lock();
	for (auto conn: m_connList) {
		mysql_close(conn);
	}
	m_curConn = 0;
	m_freeConn = 0;
	m_connList.clear();
	m_lock.unlock();
}

ConnectionPool* ConnectionPool::getInstance()
{
	static ConnectionPool connPool;
	return &connPool;
}

void ConnectionPool::initPool(string url, string user, string password, string dbName, int port, int maxConn, int closeLog)
{
	m_url = url;
	m_port = to_string(port);
	m_user = user;
	m_password = password;
	m_dbName = dbName;
	m_closeLog = closeLog;

	/* Establish maxConn connections to the specified database in advance */
	for (int i = 0; i < maxConn; ++i) {
		MYSQL* conn = nullptr;
		conn = mysql_init(conn);
		if (conn == nullptr) {
			LOG_ERROR("MySQL mysql_init error");
			exit(1);
		}
		conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), dbName.c_str(), port, nullptr, 0);
		if (conn == nullptr) {
			LOG_ERROR("MySQL mysql_real_connect error");
			exit(1);
		}
		m_connList.push_back(conn);
		m_freeConn++;
	}
	m_reserve = Sem(m_freeConn);
	m_maxConn = m_freeConn;
}

ConnectionPool::ConnectionPool()
{
	m_curConn = 0;
	m_freeConn = 0;
}

ConnectionPool::~ConnectionPool()
{
}

ConnectionRAII::ConnectionRAII(MYSQL** conn, ConnectionPool* connPool)
{
	*conn = connPool->getConnection();
	m_connRAII = *conn;
	m_poolRAII = connPool;
}

ConnectionRAII::~ConnectionRAII()
{
	m_poolRAII->releaseConnection(m_connRAII);
}