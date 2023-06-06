#include "HttpConn.h"
using namespace std;

/* Define some status information for HTTP responses */
const char* OK_200_TITLE = "OK";
const char* ERROR_400_TITLE = "Bad Request";
const char* ERROR_400_FORM = "ERROR_400: Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* ERROR_403_TITLE = "Forbidden";
const char* ERROR_403_FORM = "ERROR_403: You do not have permission to get file from this server.\n";
const char* ERROR_404_TITLE = "Not Found";
const char* ERROR_404_FORM = "ERROR_404: The requested file was not found on this server.\n";
const char* ERROR_500_TITLE = "Internal Error";
const char* ERROR_500_FORM = "ERROR_500: There was an unusual problem serving the requested file.\n";

/* Record usernames and passwords */
static unordered_map<string, string> m_users;
/* Lock */
static Locker m_lock;

/* Set file descriptor to non-blocking mode */
static int setNonblocking(int fd)
{
	int oldOption = fcntl(fd, F_GETFL);
	int newOption = oldOption | O_NONBLOCK;
	fcntl(fd, F_SETFL, newOption);
	return oldOption;
}

/* Register file descriptor events in the epoll kernel event table */
void addfd(int epollfd, int fd, bool oneShot, TriggerMode mode)
{
	epoll_event event;
	event.data.fd = fd;

	if (mode == EPOLL_ET) {
		event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	}
	else {
		event.events = EPOLLIN | EPOLLRDHUP;
	}
	if (oneShot) {
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setNonblocking(fd);
}

/* Remove file descriptor's registered events from the epoll kernel event table */
void removefd(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
	close(fd);
}

static void modfd(int epollfd, int fd, int ev, TriggerMode mode)
{
	epoll_event event;
	event.data.fd = fd;

	if (mode == EPOLL_ET) {
		event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	}
	else {
		event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
	}
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/* Static member variables of the class must be initialized outside the class */
int HttpConn::m_userCount = 0;
int HttpConn::m_epollfd = -1;

void HttpConn::closeConn(bool realClose)
{
	if (realClose && m_sockfd != -1) {
		removefd(m_epollfd, m_sockfd);
		m_sockfd = -1;
		m_userCount--;	/* Decrease the total number of clients when closing a connection */
		printf("close %d\n", m_sockfd);
	}
}
void HttpConn::init(int sockfd, const sockaddr_in& addr, char* root, TriggerMode mode, 
			int closeLog, string user, string password, string dbName)
{
	m_sockfd = sockfd;
	m_address = addr;

	addfd(m_epollfd, sockfd, true, mode);
	m_userCount++;

	/* When the browser resets, it may be due to an error in the website root directory,
       an HTTP response error, or an empty file being accessed */
	m_docRoot = root;
	m_mode = mode;
	m_closeLog = closeLog;

	/* Initialize database-related parameters */
	strcpy(m_dbUser, user.c_str());
	strcpy(m_dbPassword, password.c_str());
	strcpy(m_dbName, dbName.c_str());

	init();
}

void HttpConn::init()
{
	m_mysql = nullptr;
	m_checkState = CHECK_STATE_REQUESTLINE;
	m_linger = false;
	m_method = GET;
	m_url = nullptr;
	m_version = nullptr;
	m_contentLength = 0;
	m_host = nullptr;
	m_startLine = 0;
	m_checkedIdx = 0;
	m_readIdx = 0;
	m_writeIdx = 0;
	m_bytesToSend = 0;
	m_bytesHaveSend = 0;
	m_timerFlag = 0;
	m_improv = 0;
	m_state = 0;
	bzero(m_readBuf, READ_BUFFER_SIZE);
	bzero(m_writeBuf, WRITE_BUFFER_SIZE);
	bzero(m_realFile, FILENAME_LEN);
}

sockaddr_in* HttpConn::getAddress()
{
	return &m_address;
}

void HttpConn::initMysqlResult(ConnectionPool* connPool)
{
	/* Get a connection from the connection pool */
	MYSQL* mysql = nullptr;
	ConnectionRAII mysqlConn(&mysql, connPool);

	/* Retrieve username and password data from the user table */
	if (mysql_query(mysql, "SELECT username, passwd FROM user;")) {
		LOG_ERROR("mysql query error: %s\n", mysql_error(mysql));
	}
	/* Retrieve the complete result set from the table */
	MYSQL_RES* result = mysql_store_result(mysql);
	/* Record usernames and passwords of all clients using a hash table */
	while (MYSQL_ROW row = mysql_fetch_row(result)) {
		string name(row[0]);
		string pwd(row[1]);
		m_users[name] = pwd;
	}
}

/* Slave state machine */
HttpConn::LINE_STATUS HttpConn::parseLine()
{
	char temp;
	for (; m_checkedIdx < m_readIdx; ++m_checkedIdx) {
		temp = m_readBuf[m_checkedIdx];
		if (temp == '\r') {
			if (m_checkedIdx + 1 == m_readIdx) {
				return LINE_OPEN;
			}
			else if (m_readBuf[m_checkedIdx + 1] == '\n') {
				m_readBuf[m_checkedIdx++] = '\0';
				m_readBuf[m_checkedIdx++] = '\0';
				return LINE_OK;
			}
		}
		else if (temp == '\n') {
			if (m_checkedIdx > 1 && m_readBuf[m_checkedIdx - 1] == '\r') {
				m_readBuf[m_checkedIdx - 1] = '\0';
				m_readBuf[m_checkedIdx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

/* Read customer data in a loop until there is no data to read or the other party closes the connection */
bool HttpConn::readn()
{
	if (m_readIdx >= READ_BUFFER_SIZE) {
		return false;
	}
	int bytesRead = 0;

    /* Read data in LT mode */
	if (m_mode == EPOLL_LT) {
		bytesRead = recv(m_sockfd, m_readBuf + m_readIdx, READ_BUFFER_SIZE - m_readIdx, 0);
		if (bytesRead <= 0) {
			return false;
		}
		m_readIdx += bytesRead;
	}
	/* Read data in ET mode, read all data at once */
	else {
		while (true) {
			bytesRead = recv(m_sockfd, m_readBuf + m_readIdx, READ_BUFFER_SIZE - 
							m_readIdx, 0);
			if (bytesRead == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					break;
				}
				return false;
			}
			else if (bytesRead == 0) {
				return false;
			}
			m_readIdx += bytesRead;
		}
	}
	return true;
}

/* Parse the HTTP request line to obtain the request method, target URL, and HTTP version number */
HttpConn::HTTP_CODE HttpConn::parseRequestLine(char* text)
{
	m_url = strpbrk(text, " \t");
	if (m_url == nullptr) {
		return BAD_REQUEST;
	}
	*m_url++ = '\0';
	
	char* method = text;
	if (strcasecmp(method, "GET") == 0) {
		m_method = GET;
	}
	else if (strcasecmp(method, "POST") == 0) {
		m_method = POST;
	}
	else {
		return BAD_REQUEST;
	}

	/* Skip spaces and tabs in between */
	m_url += strspn(m_url, " \t");
	m_version = strpbrk(m_url, " \t");
	if (m_version == nullptr) {
		return BAD_REQUEST;
	}
	*m_version++ = '\0';
	m_version += strspn(m_version, " \t");
	if (strcasecmp(m_version, "HTTP/1.1") != 0) {
		return BAD_REQUEST;
	}
	if (strncasecmp(m_url, "http://", 7) == 0) {
		m_url += 7;
		m_url = strchr(m_url, '/');
	}
	if (strncasecmp(m_url, "https://", 8) == 0) {
		m_url += 8;
		m_url = strchr(m_url, '/');
	}

	if (!m_url || m_url[0] != '/') {
		return BAD_REQUEST;
	}
	/* Set the default access page when the URL address is set to '/' */
	if (strlen(m_url) == 1) {
		strcat(m_url, "judge.html");
	}

	m_checkState = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

/* Parse a header information of the HTTP request */
HttpConn::HTTP_CODE HttpConn::parseHeaders(char* text)
{
	/* When encountering an empty line, it indicates that the header field parsing is complete */
	if (text[0] == '\0') {
		/* If the HTTP request has a request body, the state machine transitions to CHECK_STATE_CONTENT */	
		if (m_contentLength != 0) {
			m_checkState = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		/* Otherwise, we have received a complete HTTP request */
		return GET_REQUEST;
	}
	/* Handle the Connection header field */
	else if (strncasecmp(text, "Connection:", 11) == 0) {
		text += 11;
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-alive") == 0) {
			m_linger = true;
		}
	}
	/* Handle the Content-Length header field */
	else if (strncasecmp(text, "Content-Length:", 15) == 0) {
		text += 15;
		text += strspn(text, " \t");
		m_contentLength = atoi(text);
	}
	/* Handle the HOST header field */
	else if (strncasecmp(text, "Host:", 5) == 0) {
		text += 5;
		text += strspn(text, " \t");
		m_host = text;
	}
	else {
		// LOG_INFO("oop! unknown header %s\n", text);
	}
	return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::parseContent(char* text)
{
	if (m_readIdx >= (m_contentLength + m_checkedIdx)) {
		text[m_contentLength] = '\0';
		/* The content of the message body in the POST request is the user name and password entered by the user */
		m_namePassword = text;
		return GET_REQUEST;
	}
	return NO_REQUEST;
}

/* Main state machine */
HttpConn::HTTP_CODE HttpConn::processRead()
{
	LINE_STATUS lineStatus = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = nullptr;

	while ((m_checkState == CHECK_STATE_CONTENT && lineStatus == LINE_OK) 
		 || (lineStatus = parseLine()) == LINE_OK) {
		text = getLine();
		m_startLine = m_checkedIdx;
		LOG_INFO("%s", text);

		switch (m_checkState) {
		case CHECK_STATE_REQUESTLINE:
		{
			ret = parseRequestLine(text);
			if (ret == BAD_REQUEST) {
				return BAD_REQUEST;
			}
			break;
		}
		case CHECK_STATE_HEADER:
		{
			ret = parseHeaders(text);
			if (ret == BAD_REQUEST) {
				return BAD_REQUEST;
			}
			else if (ret == GET_REQUEST) {
				return doRequest();
			}
			break;
		}
		case CHECK_STATE_CONTENT:
		{
			ret = parseContent(text);
			if (ret == GET_REQUEST) {
				return doRequest();
			}
			lineStatus = LINE_OPEN;
			break;
		}
		default:
		{
			return INTERNAL_ERROR;
		}
		}
	}
	return NO_REQUEST;
}

/* When we get a complete and correct HTTP request, we analyze the properties of the target file. If the target file exists
  * And it is readable by all users, use mmap to map it to the memory address m_fileAddress, and return success */
HttpConn::HTTP_CODE HttpConn::doRequest()
{
	strcpy(m_realFile, m_docRoot);
	int len = strlen(m_docRoot);
	
	const char* p = strrchr(m_url, '/');

	/* Handle CGI */
	if (m_method == POST) {
		if (strncasecmp(p + 1, "log.cgi", 7) == 0) {
			CGI_UserLog();
		}	
		else if (strncasecmp(p + 1, "regist.cgi", 10) == 0){
			CGI_UserRegist();
		}
		else if (strncasecmp(p + 1, "musiclist.cgi", 13) == 0) {
			CGI_MusicList();
		}
	}
	strncpy(m_realFile + len, m_url, FILENAME_LEN - len - 1);

	if (stat(m_realFile, &m_fileStat) < 0) {
		return NO_RESOURCE;
	}

	if (!(m_fileStat.st_mode & S_IROTH)) {
		return FORBIDDEN_REQUEST;
	}

	if (S_ISDIR(m_fileStat.st_mode)) {
		return BAD_REQUEST;
	}

	int fd = open(m_realFile, O_RDONLY);
	m_fileAddress = (char*)mmap(0, m_fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	return FILE_REQUEST;
}

/* Perform munmap operation on the memory map area */
void HttpConn::unmap()
{
	if (m_fileAddress) {
		munmap(m_fileAddress, m_fileStat.st_size);
		m_fileAddress = nullptr;
	}
}

/* Write HTTP response */
bool HttpConn::writen()
{
	int temp = 0;
	if (m_bytesToSend == 0) {
		modfd(m_epollfd, m_sockfd, EPOLLIN, m_mode);
		init();
		return true;
	}
	
	while (true) {
		temp = writev(m_sockfd, m_iv, m_ivCount);
		if (temp <= -1) {
			/* If there is no space in the TCP write buffer, wait for the next round of EPOLLOUT events */
			if (errno == EAGAIN) {
				modfd(m_epollfd, m_sockfd, EPOLLOUT, m_mode);
				return true;
			}
			unmap();
			return false;
		}

		m_bytesToSend -= temp;
		m_bytesHaveSend += temp;
		/* Solve the problem of transferring large files */
		if (m_bytesHaveSend >= m_iv[0].iov_len) {
			m_iv[0].iov_len = 0;
			m_iv[1].iov_base = m_fileAddress + (m_bytesHaveSend - m_writeIdx);
			m_iv[1].iov_len = m_bytesToSend;
		}
		else {
			m_iv[0].iov_base = m_writeBuf + m_bytesHaveSend;
			m_iv[0].iov_len = m_iv[0].iov_len - m_bytesHaveSend;
		}

		/* Send the HTTP response successfully, decide whether to close the connection immediately according to the Connection field in the HTTP request */
		if (m_bytesToSend <= 0) {
			unmap();
			modfd(m_epollfd, m_sockfd, EPOLLIN, m_mode);
			if (m_linger) {
				init();
				return true;
			}
			else {
				return false;
			}
		}
	}
}

/* Write the data to be sent to the write buffer */
bool HttpConn::addResponse(const char* format, ...)
{
	if (m_writeIdx >= WRITE_BUFFER_SIZE) {
		return false;
	}
	va_list argList;
	va_start(argList, format);
	int len = vsnprintf(m_writeBuf + m_writeIdx, WRITE_BUFFER_SIZE - 1 - m_writeIdx,
				        format, argList);
	if (len >= WRITE_BUFFER_SIZE - 1 - m_writeIdx) {
		va_end(argList);
		return false;
	}
	m_writeIdx += len;
	va_end(argList);

	LOG_INFO("request: %s", m_writeBuf);
	return true;
}

bool HttpConn::addStatusLine(int status, const char* title)
{
	return addResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConn::addHeaders(int contentLength)
{
	addContentLength(contentLength);
	addLinger();
	addBlankLine();
	return true;
}

bool HttpConn::addContentLength(int contentLength)
{
	return addResponse("Content-Length: %d\r\n", contentLength);
}

bool HttpConn::addLinger()
{
	return addResponse("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool HttpConn::addBlankLine()
{
	return addResponse("%s", "\r\n");
}

bool HttpConn::addContent(const char* content)
{
	return addResponse("%s", content);
}

/* Determine the content returned to the client according to the result of the server processing the HTTP request */
bool HttpConn::processWrite(HTTP_CODE ret)
{
	switch (ret) {
		case INTERNAL_ERROR:
		{
			addStatusLine(500, ERROR_500_TITLE);
			addHeaders(strlen(ERROR_500_FORM));
			if (!addContent(ERROR_500_FORM)) {
				return false;
			}
			break;
		}
		case BAD_REQUEST:
		{
			addStatusLine(400, ERROR_400_TITLE);
			addHeaders(strlen(ERROR_400_FORM));
			if (!addContent(ERROR_400_FORM)) {
				return false;
			}
			break;
		}
		case NO_RESOURCE:
		{
			addStatusLine(404, ERROR_404_TITLE);
			addHeaders(strlen(ERROR_404_FORM));
			if (!addContent(ERROR_404_FORM)) {
				return false;
			}
			break;
		}
		case FORBIDDEN_REQUEST:
		{
			addStatusLine(403, ERROR_403_TITLE);
			addHeaders(strlen(ERROR_403_FORM));
			if (!addContent(ERROR_403_FORM)) {
				return false;
			}
			break;
		}
		case FILE_REQUEST:
		{
			addStatusLine(200, OK_200_TITLE);
			if (m_fileStat.st_size != 0) {
				addHeaders(m_fileStat.st_size);
				m_iv[0].iov_base = m_writeBuf;
				m_iv[0].iov_len = m_writeIdx;
				m_iv[1].iov_base = m_fileAddress;
				m_iv[1].iov_len = m_fileStat.st_size;
				m_ivCount = 2;
				m_bytesToSend = m_writeIdx + m_fileStat.st_size;
				return true;
			}
			else {
				const char* OK_STRING = "<html><body></body></html>";
				addHeaders(strlen(OK_STRING));
				if (!addContent(OK_STRING)) {
					return false;
				}
			}
		}
		default:
		{
			return false;
		}
	}
	m_iv[0].iov_base = m_writeBuf;
	m_iv[0].iov_len = m_writeIdx;
	m_ivCount = 1;
	m_bytesToSend = m_writeIdx;
	return true;
}

/* Called by the worker thread in the thread pool, this is the entry function for processing HTTP requests */
void HttpConn::process()
{
	HTTP_CODE readRet = processRead();
	if (readRet == NO_REQUEST) {
		modfd(m_epollfd, m_sockfd, EPOLLIN, m_mode);
		return;
	}
	bool writeRet = processWrite(readRet);
	if (!writeRet) {
		closeConn();
	}
	modfd(m_epollfd, m_sockfd, EPOLLOUT, m_mode);
}

void HttpConn::CGI_UserLog()
{
	/* First parse the username and password from the request body content */
	string name, password;
	getNameAndPwd(name, password);
	if (m_users.find(name) != m_users.end() && m_users[name] == password) {
		strcpy(m_url, "/welcome.html");
	}
	else {
		strcpy(m_url, "/logError.html");
	}
}

void HttpConn::CGI_UserRegist()
{
	/* First parse the username and password from the request body content */
	string name, password;
	getNameAndPwd(name, password);
	/* First check if there is a duplicate name in the database */
	/* If there is no duplicate name, insert it directly */
	if (m_users.find(name) == m_users.end()) {
		char sql[200] = { 0 };
		sprintf(sql, "INSERT INTO user(username, passwd) VALUES('%s', '%s');", name.c_str(), password.c_str());
		m_lock.lock();
		int res = mysql_query(m_mysql, sql);
		m_users[name] = password;
		m_lock.unlock();
		if (!res) {
			strcpy(m_url, "/log.html");
		}
		else {
			strcpy(m_url, "/registError.html");
		}
	}
	else {
		strcpy(m_url, "/registError.html");
	}
}

int HttpConn::getNameAndPwd(string& name, string& password)
{
    // user=123&password=123
	int i;
	for (i = 5; m_namePassword[i] != '&'; ++i) {
		name.push_back(m_namePassword[i]);
	}
	for (i = i + 10; i < m_namePassword.length(); ++i) {
		password.push_back(m_namePassword[i]);
	}
	return true;
}

void HttpConn::CGI_MusicList()
{
	char tempBuf[BUFSIZ] = { 0 };
	int len = 0;
	/* Read the header of the html file */
	int fd = open("root/dir_header.html", O_RDONLY);
	if (fd == -1) {
		LOG_ERROR("CGI_MusicList open error, errno is %d", errno);
		return;
	}
	size_t headerLen = read(fd, tempBuf, sizeof(tempBuf));
	if (headerLen == -1) {
		LOG_ERROR("CGI_MusicList read error, errno is %d", errno);
		return;
	}
	close(fd);
	len += headerLen;

	/* Query all information under the music folder */
	struct dirent** nameList;
	int num = scandir("root/music", &nameList, nullptr, alphasort);
	if (num < 0) {
		LOG_ERROR("CGI_MusicList scandir error, errno is %d", errno);
		return;
	}
	/* Write all file connections under the music folder into the buffer */
	while (num--) {
		if (nameList[num]->d_name[0] == '.') {
			continue;
		}
		int n = snprintf(tempBuf + len, BUFSIZ - len, "<li><a href=music/%s>%s</a></li>\n", nameList[num]->d_name, nameList[num]->d_name);
		len += n;
		free(nameList[num]);
	}
	free(nameList);

	/* Read the end of the html file */
	fd = open("root/dir_tail.html", O_RDONLY);
	if (fd == -1) {
		LOG_ERROR("CGI_MusicList open error, errno is %d", errno);
		return;
	}
	size_t tailLen = read(fd, tempBuf + len, sizeof(tempBuf) - len);
	if (tailLen == -1) {
		LOG_ERROR("CGI_MusicList read error, errno is %d", errno);
		return;
	}
	close(fd);
	len += tailLen;
	/* generate dynamic html file */
	FILE* fp = fopen("root/musiclist.html", "w+");
	if (fp == nullptr) {
		LOG_ERROR("CGI_MusicList open error, errno is %d", errno);
		return;
	}
	int ret = fputs(tempBuf, fp);
	if (ret == EOF) {
		LOG_ERROR("CGI_MusicList open error, errno is %d", errno);
		return;
	}
	fflush(fp);
	fclose(fp);
	strcpy(m_url, "/musiclist.html");
}