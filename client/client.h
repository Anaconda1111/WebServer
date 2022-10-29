#ifndef CLIENT_H__
#define CLIENT_H__

#include"common/util.h"
#include"common/timer.h"
#include"common/mutex.h"
#include"database/mysql_pool.h"
#include <sys/stat.h>
#include <string>
#include <unordered_map>

typedef enum{
    READ=0,
    WRITE,
    CLOSE
} CLIENT_EVENT_TYPE;

typedef enum{
    GET=0,
    POST
} HTTP_METHOD;

typedef  enum{
    NO_REQUEST=0,
    BAD_REQUEST,
    GET_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSE_CONNECTION
} HTTP_CODE;

typedef enum{
    CHECK_REQUEST_LINE=0,
    CHECK_HEADER,
    CHECK_CONTENT
} CHECK_STATE;

typedef enum{
    LINE_OK=0,
    LINE_BAD,
    LINE_OPEN
} LINE_STATE;

const int READ_BUFFER_SIZE=2048;
const int WRITE_BUFFER_SIZE=1024;
const int FILE_NAME_SIZE=200;
class Client{

public:
    Client(int epoll_fd,int sock_fd);
    ~Client();
    void init(MysqlConnectionPool*,Timer*,int64_t,int,int);//
    int closeConnection();//
    int64_t getTimerID(){return timer_id;}
    bool checkConnectionValid() {return socket_fd>=0;}
    static std::unordered_map<std::string,std::string>* user_dict;

private:

    struct resourceName{
        const char* judge;
        const char* login;
        const char* registerError;
        const char* logError;
        const char* welcome;
        const char* regist;
        const char* picture;
        const char* video;
        const char* fans;

        resourceName(){
            judge="/judge.html";
            login="/log.html";
            registerError="/registerError.html";
            logError="/logError.html";
            welcome="/welcome.html";
            regist="/register.html";
            picture="/picture.html";
            video="/video.html";
            fans="/fans.html";
        }
    };
    
    int socket_fd;
    int port;
    int epoll_fd;
    int64_t timer_id;
    
    HTTP_METHOD method;
    CHECK_STATE check_state;
    CLIENT_EVENT_TYPE event_type;
    

    static std::string file_path_root;
    static struct resourceName resource_name;

    
    Mutex user_dict_lock;

    MysqlConnectionPool* mysql_pool_ptr;
    Timer* timer_ptr;
    time_t last_event_happen;

    char file_name[FILE_NAME_SIZE];

    void init();//



    Client(const Client&);
    Client& operator=(const Client&);
    Client(const Client&&);
    Client& operator=(const Client&&);

//----------------------读事件相关---------------------

private:

    char read_buffer[READ_BUFFER_SIZE];
    int  read_pos;
    int  check_pos;
    int  line_start;
    int  content_len;
    char* url;
    char* host;
    char* version;
    char* content;
    bool cgi;
    bool keep_alive;

    char*getLine(){return read_buffer+line_start;}

    bool getDataFromSocketBuffer();           //从内核缓冲区中读取数据到用户缓冲区
    LINE_STATE praseLine();                   //解析一行
    HTTP_CODE praseRequestLine(char* text);   //解析请求行
    HTTP_CODE praseHeader(char* text);        //解析首部行
    HTTP_CODE praseContent(char* text);       //解析报文主体：仅添加\0字符以及设置content成员为报文主体的初始位置
    HTTP_CODE requestHandle();                //根据解析结果，设置对应资源文件路经/登录/注册    
    HTTP_CODE analysisHttpMessage();          //状态机解析HTTP报文
public:
    void readEventHandle();              //
    void flushEventHapenTime();
    bool isOverTime();

//-------------------------------------------------------


//----------------------写事件相关---------------------
private:

    char         write_buffer[WRITE_BUFFER_SIZE];
    int          write_pos;
    int          byte_need_send;
    int          byte_has_send;

    struct stat  file_stat;
    char*        file_addr;

    struct iovec io_vec[2];
    int          iovec_count;

    bool addRespone(const char*format,...);           //往用户态写缓冲区中写入一行数据
    bool addStaueLine(int staue,const char* title);   //添加响应报文状态行
    bool addHeader(int content_len);                  //添加头部字段，调用所有写入头部字段的函数
    bool addContentLen(int content_len);              //添加字段 Content-Length
    bool addContentType();                            //添加字段 Content-Type
    bool addKeepAlive();                              //添加字段 Connection
    bool addBlankLine();                              //添加空白行
    bool addContent(const char*);                     //添加报文主体
    bool generalHttpMessage(HTTP_CODE);               //创建HTTP报文（报文主体不包括除了错误时的字符串外的其他东西，如视频、图片等)

    bool sendDataToSocketBuffer();   //将数据写入内核socket缓冲区

    void close_mmap();               //关闭文件映射

public:

    void writeEventHandle();

//-------------------------------------------------------
};

#endif