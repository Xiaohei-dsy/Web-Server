#ifndef _CONFIG_H__
#define _CONFIG_H__

#include <iostream>
#include "WebServer.h"
using namespace std;

class Config
{
public:
	Config();

	void parseArg(int argc, char* argv[]);
    /* Port number */
    int port;
    /* Log writing method */
    int logWrite;
    /* Trigger combination mode */
    int triggerMode;
    /* lfd trigger mode */
    TriggerMode lfdMode;
    /* cfd trigger mode */
    TriggerMode cfdMode;
    /* Graceful shutdown of connections */
    int optLinger;
    /* Number of database connection pools */
    int sqlNum;
    /* Number of threads in the thread pool */
    int threadNum;
    /* Whether to disable logging */
    int closeLog;
    /* Concurrent model selection */
    ActorModel model;

};

#endif