 Quick Start
------------

* Make sure MySQL database is installed before testing

    ```SQL
    // Create yourdb database
    create database yourdb;

    // Create user table
    USE yourdb;
    CREATE TABLE user(
        username char(50) NULL,
        passwd char(50) NULL
    )ENGINE=InnoDB;

    // Add data
    INSERT INTO user(username, passwd) VALUES('name', 'passwd');
    ```

* Modify the database initialization information in main.cpp

    ```C++
    // Database login name, password, database name
    string user = "root";
    string passwd = "root";
    string databasename = "yourdb";
    ```

* Build (Note, if your make fails, you may need to change the path in the Makefile according to your MySQL installation location)

    ```C++
    make 
    ```

* Start the server

    ```C++
    ./WebServer
    ```

* Browser

    ```C++
    ip:9007
    ```

Customized Run
------

```C++
./server [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model]
```

Note: The above parameters are optional, you don't have to use all of them. Use them according to your needs.

-p, custom port number
    Default: 8888
    
-l, choose logging mode (default: synchronous)
    0: synchronous logging
    1: asynchronous logging
    
-m, combination mode for listenfd and connfd (default: ET + ET)
    0: LT + LT
    1: LT + ET
    2: ET + LT
    3: ET + ET
    
-o, graceful connection closure (default: disabled)
    0: disabled
    1: enabled
    
-s, number of database connections (default: 8)
    
-t, number of threads (default: 8)
    
-c, close logging (default: disabled)
    0: enable logging
    1: disable logging
    
-a, reactor model (default: Proactor)
    0: Proactor model
    1: Reactor model