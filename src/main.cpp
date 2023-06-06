#include <iostream>
#include "Config.h"
using namespace std;

int main(int argc, char* argv[])
{
    /* Database information that needs to be modified */
    string dbUser = "root";
    string dbPassword = "282244952";
    string dbName = "db_670";

    /* Command line parsing */
    Config config;
    config.parseArg(argc, argv);

    /* Server initialization */
    WebServer server;

    /* Enter daemon process */
    server.initDaemon();

    server.init(config.port, dbUser, dbPassword, dbName, config.logWrite, config.optLinger, config.triggerMode, 
                config.sqlNum, config.threadNum, config.closeLog, config.model);

    /* Log */
    server.logWriteInit();

    /* Database connection pool */
    server.connectionPoolInit();

    /* Thread pool */
    server.threadPoolInit();

    /* Trigger mode */
    server.trigMode();

    /* Set up listening */
    server.eventListen();

    /* Run */
    server.eventLoop();

    return 0;
}