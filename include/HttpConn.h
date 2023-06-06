#ifndef _HTTP_CONN_H__
#define _HTTP_CONN_H__

#include <string>
#include <unordered_map>
#include "Web.h"
#include "Locker.h"
#include "ConnectionPool.h"
using namespace std;

struct UserInfo
{
    UserInfo(string name, string pwd): userName(name), password(pwd) {}
    string userName;
    string password;
};

class HttpConn
{
public:
    /* Maximum length of the filename */
    static constexpr int FILENAME_LEN = 200;
    /* Size of the read buffer */
    static constexpr int READ_BUFFER_SIZE = 2048;
    /* Size of the write buffer */
    static constexpr int WRITE_BUFFER_SIZE = 1024;
    /* HTTP request methods */
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE,
                  TRACE, OPTIONS, CONNECT, PATCH };
    /* States of the main state machine during parsing client requests */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0,
                       CHECK_STATE_HEADER,
                       CHECK_STATE_CONTENT };
    /* Possible results of the server processing an HTTP request */
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST,
                     NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,
                     INTERNAL_ERROR, CLOSED_CONNECTION };
    /* Line reading status */
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

public:
    HttpConn(){};
    ~HttpConn(){};

public:
    /* Initialize a newly accepted connection */
    void init(int sockfd, const sockaddr_in& addr, char* root, TriggerMode mode,
            int closeLog, string user, string password, string dbName);
    /* Close the connection */
    void closeConn(bool realClose = true);
    /* Process client request */
    void process();
    /* Non-blocking read operation */
    bool readn();
    /* Non-blocking write operation */
    bool writen();
    /* Get the client's socket address structure */
    sockaddr_in* getAddress();
    /* Pre-read all user information from the database */
    void initMysqlResult(ConnectionPool* connPool);

private:
    /* Initialize the connection */
    void init();
    /* Parse the HTTP request */
    HTTP_CODE processRead();
    /* Populate the HTTP response */
    bool processWrite(HTTP_CODE ret);

    /* The following set of functions are called by processRead to analyze the HTTP request */
    HTTP_CODE parseRequestLine(char* text);
    HTTP_CODE parseHeaders(char* text);
    HTTP_CODE parseContent(char* text);
    HTTP_CODE doRequest();
    char* getLine() { return m_readBuf + m_startLine; }
    LINE_STATUS parseLine();

    /* The following set of functions are called by processWrite to populate the HTTP response */
    void unmap();
    bool addResponse(const char* format, ...);
    bool addContent(const char* content);
    bool addStatusLine(int status, const char* title);
    bool addHeaders(int contentLength);
    bool addContentLength(int contentLength);
    bool addLinger();
    bool addBlankLine();

    /* Parse the submitted username and password from the POST request body */
    int getNameAndPwd(string& name, string& password);
    /* Handle different CGI methods */
    void CGI_UserLog();
    void CGI_UserRegist();
    void CGI_MusicList();


public:
    /* All events on sockets are registered to the same epoll kernel event table */
    static int m_epollfd;
    /* Count of connected users */
    static int m_userCount;

    int m_timerFlag;
    int m_improv;
    /* Database connection handle */
    MYSQL* m_mysql;
    /* Read: 0, Write: 1 */
    int m_state;

private:
    /* Connection socket */
    int m_sockfd;
    /* Socket address of the other end */
    sockaddr_in m_address;
    /* Read buffer */
    char m_readBuf[READ_BUFFER_SIZE];
    /* Index of the last byte of client data that has been read into the read buffer */
    int m_readIdx;
    /* Index of the current character being analyzed in the read buffer */
    int m_checkedIdx;
    /* Start position of the line currently being parsed */
    int m_startLine;
    /* Write buffer */
    char m_writeBuf[WRITE_BUFFER_SIZE];
    /* Number of bytes waiting to be sent in the write buffer */
    int m_writeIdx;

    /* Current position of the main state machine */
    CHECK_STATE m_checkState;
    /* Request method */
    METHOD m_method;

    /* Root directory of the website */
    char* m_docRoot;
    /* Full path of the target file requested by the client, equivalent to the website root directory + m_url */
    char m_realFile[FILENAME_LEN];
    /* File name of the target file requested by the client */
    char* m_url;
    /* HTTP protocol version number, only support HTTP/1.1 */
    char* m_version;
    /* Host name */
    char* m_host;
    /* Length of the HTTP request message body */
    int m_contentLength;
    /* Whether the HTTP request requires keeping the connection alive */
    bool m_linger;
    /* EPOLL trigger mode */
    TriggerMode m_mode;
    /* Whether logging is disabled */
    int m_closeLog;
    /* Whether POST is enabled */
    // int m_cgi;

    /* Starting position in memory where the target file requested by the client is mmap'ed */
    char* m_fileAddress;
    /* Status of the target file (whether it exists, whether it is a directory, whether it is readable, etc.) */
    struct stat m_fileStat;
    /* Use writev to perform write operations */
    struct iovec m_iv[2];
    /* Number of memory blocks being written */
    int m_ivCount;
    /* Number of bytes to be sent from the buffer */
    size_t m_bytesToSend;
    /* Number of bytes already sent from the buffer */
    size_t m_bytesHaveSend;

    /* Database username */
    char m_dbUser[100];
    /* Database password */
    char m_dbPassword[100];
    /* Database name */
    char m_dbName[100];
    /* Temporary storage of the content (username and password) of the POST request message body */
    string m_namePassword;
};

#endif