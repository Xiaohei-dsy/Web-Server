#include <iostream>
#include "Config.h"
using namespace std;

Config::Config()
{
	/* Port number, default is 9007 */
	port = 9007;
	/* Log write mode, default is synchronous */
	logWrite = 0;
	/* Trigger combination mode, default is listenfd ET + connfd ET */
	triggerMode = 3;
	/* listenfd trigger mode, default is LT */
	lfdMode = EPOLL_LT;
	/* connfd trigger mode, default is LT */
	cfdMode = EPOLL_LT;
	/* Graceful connection closure, default is not used */
	optLinger = 0;
	/* Database connection pool size, default is 8 */
	sqlNum = 8;
	/* Number of threads in the thread pool, default is 8 */
	threadNum = 8;
	/* Close logging, default is closed */
	closeLog = 1;
	/* Server concurrency model, default is proactor */
	model = PROACTOR;
}

void Config::parseArg(int argc, char* argv[])
{
	int opt;
	const char* str = "p:l:m:o:s:t:c:a:";
	while ((opt = getopt(argc, argv, str)) != -1) {
		switch (opt)
		{
		case 'p':
			port = atoi(optarg);
			break;
		case 'l':
			logWrite = atoi(optarg);
			break;
		case 'm':
			triggerMode = atoi(optarg);
			break;
		case 'o':
			optLinger = atoi(optarg);
			break;
		case 's':
			sqlNum = atoi(optarg);
			break;
		case 't':
			threadNum = atoi(optarg);
			break;
		case 'c':
			closeLog = atoi(optarg);
			break;
		case 'a':
		{
			int tmp = atoi(optarg);	
			model = tmp == 0 ? PROACTOR : REACTOR;
			break;
		}
		default:
			break;
		}
	}
}
