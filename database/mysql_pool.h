#ifndef MYSQL_POOL_H__
#define MYSQL_POOL_H__

#include<vector>
#include<string>
#include<mysql/mysql.h>
#include"common/mutex.h"

class MysqlConnectionPool;

class MysqlConnectionRAII{

private:
    MYSQL* conn;
    MysqlConnectionPool* pool;
public:
    
    MysqlConnectionRAII(MysqlConnectionPool*);
    ~MysqlConnectionRAII();
    MYSQL* real_ptr(){return conn;}

};

class MysqlConnectionPool{

friend class MysqlConnectionRAII;

private:
    int maxConnection;
    int freeConnection;
    int busyConnection;

    std::string addr;
    std::string user;
    std::string password;
    std::string dbname;
    int port;

    std::vector<MYSQL*> connetions;

    Mutex m_lock; 

    bool is_init;

    MYSQL* getConnection();
    void releaseConnection(MYSQL*);

    void destory();

public:

    MysqlConnectionPool(std::string,std::string,std::string,std::string,int,int);
    ~MysqlConnectionPool();
    void init();
    

};


#endif