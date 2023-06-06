#include <iostream>
#include <pthread.h>
#include <cstring>
#include <ctime>
#include "Log.h"
using namespace std;

/* After C++11, local lazy initialization does not require locking */
Log* Log::getInstance()
{
	static Log instance;
	return &instance;
}

/* Callback function for the worker thread */
void* Log::flushLogThread(void* arg)
{
	Log::getInstance()->asyncWriteLog();
	return nullptr;
}

Log::Log()
{
	m_count = 0;
	m_isAsync = false;
}

Log::~Log()
{
	if (m_fp != nullptr) {
		fclose(m_fp);
		m_fp = nullptr;
	}
	if (m_buf != nullptr) {
		delete [] m_buf;
		m_buf = nullptr;
	}
	if (m_logQueue != nullptr) {
		delete m_logQueue;
		m_logQueue = nullptr;
	}
}

/* Asynchronous mode requires setting the block queue length, synchronous mode does not require setting it */
bool Log::init(const char* fileName, int closeLog, int logBufSize, int splitLines, int maxQueueSize)
{
	/* If the block queue length is set, set it as asynchronous */
	if (maxQueueSize > 0) {
		m_isAsync = true;
		m_logQueue = new BlockQueue<string>(maxQueueSize);
		pthread_t tid;
		pthread_create(&tid, nullptr, flushLogThread, nullptr);
	}

	m_closeLog = closeLog;
	m_logBufSize = logBufSize;
	m_buf = new char[m_logBufSize];
	memset(m_buf, '\0', m_logBufSize);
	m_spiltLines = splitLines;

	time_t t = time(nullptr);
	struct tm* sysTm = localtime(&t);
	struct tm myTm = *sysTm;

	/* Pointer p points to the last occurrence of '/' */
	const char* p = strrchr(fileName, '/');
	char logFullName[301] = { 0 };

	if (p == nullptr) {
		snprintf(logFullName, 300, "%d_%02d_%02d_%s", myTm.tm_year + 1900, 
				myTm.tm_mon + 1, myTm.tm_mday, fileName);
	}
	else {
		strcpy(m_logName, p + 1);
		strncpy(m_dirName, fileName, p - fileName + 1);
		snprintf(logFullName, 300, "%s%d_%02d_%02d_%s", m_dirName, myTm.tm_year + 1900, 
				myTm.tm_mon + 1, myTm.tm_mday, m_logName);	
	}
	m_today = myTm.tm_mday;

	m_fp = fopen(logFullName, "a");
	if (m_fp == nullptr) {
		return false;
	}
	return true;
}

void Log::writeLog(Level level, const char* format, ...)
{
	struct timeval now = { 0, 0 };
	gettimeofday(&now, nullptr);
	time_t t = now.tv_sec;
	struct tm* sysTm = localtime(&t);
	struct tm myTm = *sysTm;
	char s[16] = { 0 };

	switch (level)
	{
	case DEBUG:
		strcpy(s, "[debug]: ");
		break;
	case INFO:
		strcpy(s, "[info]: ");
		break;
	case WARN:
		strcpy(s, "[warn]: ");
		break;
	case ERROR:
		strcpy(s, "[error]: ");
		break;
	default:
		strcpy(s, "[info]: ");
		break;
	}

	/* Write a log and increment m_count */
	m_mutex.lock();
	m_count++;
	if (m_today != myTm.tm_mday || m_count % m_spiltLines == 0) {
		char newLog[301] = { 0 };
		fflush(m_fp);
		fclose(m_fp);
		char tail[16] = { 0 };
		snprintf(tail, 16, "%d_%02d_%02d_", myTm.tm_year + 1900, myTm.tm_mon + 1, myTm.tm_mday);
		if (m_today != myTm.tm_mday) {
			snprintf(newLog, 300, "%s%s%s", m_dirName, tail, m_logName);
			m_today = myTm.tm_mday;
			m_count = 0;
		}
		else {
			snprintf(newLog, 300, "%s%s%s.%lld", m_dirName, tail, m_logName, m_count / m_spiltLines);
		}
		m_fp = fopen(newLog, "a");
	}
	m_mutex.unlock();

	va_list valst;
	va_start(valst, format);
	string logStr;

	m_mutex.lock();
	int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
            myTm.tm_year + 1900, myTm.tm_mon + 1, myTm.tm_mday,
            myTm.tm_hour, myTm.tm_min, myTm.tm_sec, now.tv_usec, s);
	int m = vsnprintf(m_buf + n, m_logBufSize - 1, format, valst);
	m_buf[n + m] = '\n';
	m_buf[n + m + 1] = '\0';
	logStr = m_buf;
	m_mutex.unlock();

	if (m_isAsync && !m_logQueue->isFull()) {
		m_logQueue->push(logStr);
	}
	else {
		m_mutex.lock();
		fputs(logStr.c_str(), m_fp);
		m_mutex.unlock();
	}

	va_end(valst);
}

void Log::flush(void)
{
	m_mutex.lock();
	fflush(m_fp);
	m_mutex.unlock();
}

void* Log::asyncWriteLog()
{
	string singleLog;
	/* Take a log string from the blocking queue and write it to a file */
	while (m_logQueue->pop(singleLog)) {
		m_mutex.lock();
		fputs(singleLog.c_str(), m_fp);
		m_mutex.unlock();
	}
	return nullptr;
}