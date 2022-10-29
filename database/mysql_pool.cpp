#include<assert.h>
#include"mysql_pool.h"

MysqlConnectionRAII::MysqlConnectionRAII(MysqlConnectionPool* _pool):pool(_pool){
    conn=pool->getConnection();
}

MysqlConnectionRAII::~MysqlConnectionRAII(){
    pool->releaseConnection(conn);
}

MysqlConnectionPool::MysqlConnectionPool(std::string addr,
                                        std::string user,
                                        std::string password,
                                        std::string dbname,int port,int max_conn):
                                        addr(addr),user(user),password(password),dbname(dbname),is_init(false),
                                        maxConnection(max_conn),freeConnection(max_conn),busyConnection(0){}

MysqlConnectionPool::~MysqlConnectionPool(){
    destory();
}


void MysqlConnectionPool::init(){
    for(int i=0;i<maxConnection;i++){
        MYSQL* conn=mysql_init(NULL);
        assert(conn);
        conn=mysql_real_connect(conn,addr.c_str(),user.c_str(),password.c_str(),dbname.c_str(),port,NULL,0);
        assert(conn);
        connetions.push_back(conn);
    }
    is_init=true;
}

MYSQL* MysqlConnectionPool::getConnection(){
    assert(is_init);
    AutoLock locker(&m_lock);
    if(freeConnection<=0) return nullptr;
    MYSQL* res=connetions.back();
    connetions.pop_back();
    freeConnection--;
    busyConnection++;
    return res;
}

void MysqlConnectionPool::releaseConnection(MYSQL* conn){
    assert(is_init);
    AutoLock locker(&m_lock);
    connetions.push_back(conn);
    freeConnection++;
    busyConnection--;
}

void MysqlConnectionPool::destory(){
    assert(is_init);
    AutoLock locker(&m_lock);
    for(int i=0;i<connetions.size();i++){
        mysql_close(connetions[i]);
    }
    freeConnection=busyConnection=0;
    connetions.clear();
}
