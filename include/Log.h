#ifndef _LOG_H__
#define _LOG_H__

#include <iostream>
#include <cstdio>
#include <cstdarg>
#include "BlockQueue.h"
using namespace std;

/* Singleton class for implementing a logging system */
class Log
{
public:
    /* Log printing levels */
    enum Level { DEBUG = 1, INFO, WARN, ERROR };
    /* Get the globally unique instance of this class */
    static Log* getInstance();
    /* Callback function for the working thread */
    static void* flushLogThread(void* arg);
    /* Optional parameters: log file, log buffer size, maximum number of lines, and maximum log queue size */
    bool init(const char* fileName, int closeLog, int logBufSize = 8192, int splitLines = 5000000, int maxQueueSize = 0);
    /* Print log with the specified format */
    void writeLog(Level level, const char* format, ...);
    /* Force flushing to disk */
    void flush(void);

private:
    /* Private constructor and destructor */
    Log();
    virtual ~Log();
    /* Prevent object copying */
    Log(const Log& log) = delete;
    Log& operator=(const Log& log) = delete;
    /* Asynchronously write log */
    void* asyncWriteLog();

private:
    /* Directory name where the logs are located */
    char m_dirName[128];
    /* Log file name */
    char m_logName[128];
    /* Maximum number of lines in the log */
    int m_spiltLines;
    /* Log buffer size */
    int m_logBufSize;
    /* Log line count */
    long long m_count;
    /* Used for printing logs on a daily basis to record the current day */
    int m_today;
    /* File pointer pointing to the log file */
    FILE* m_fp;
    /* Log buffer address */
    char* m_buf;
    /* Thread-safe blocking queue */
    BlockQueue<string>* m_logQueue;
    /* Flag for asynchronous logging */	
    bool m_isAsync;
    /* Mutex lock */
    Locker m_mutex;
    /* Flag for closing the log */
    int m_closeLog;
};

#define LOG_DEBUG(format, ...) 	if (m_closeLog == 0) { Log::getInstance()->writeLog(Log::DEBUG, format, ##__VA_ARGS__);	Log::getInstance()->flush(); }
#define LOG_INFO(format, ...) 	if (m_closeLog == 0) { Log::getInstance()->writeLog(Log::INFO, format, ##__VA_ARGS__); 	Log::getInstance()->flush(); }
#define LOG_WARN(format, ...) 	if (m_closeLog == 0) { Log::getInstance()->writeLog(Log::WARN, format, ##__VA_ARGS__); 	Log::getInstance()->flush(); }
#define LOG_ERROR(format, ...) 	if (m_closeLog == 0) { Log::getInstance()->writeLog(Log::ERROR, format, ##__VA_ARGS__); Log::getInstance()->flush(); }

#endif