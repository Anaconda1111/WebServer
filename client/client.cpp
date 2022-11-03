#include "client.h"
#include "log/log.h"
#include "common/util.h"
#include "config/config.h"
#include "common/time_util.h"
#include <sys/mman.h>
#include <string.h>

std::string Client::file_path_root="/mnt/hgfs/share/WebServer/root";
Client::resourceName Client::resource_name;
std::unordered_map<std::string,std::string>* Client::user_dict=nullptr;

const char* OK_200         = "OK";
const char* ERROR_400      = "Bad Request";
const char* ERROR_400_FORM = "Your request has bad syntax or inhertenly impossible to staisfy.\n";
const char* ERROR_403      = "Forbidden";
const char* ERROR_403_FORM = "You do not have permission to get file from this server.\n";
const char* ERROR_404      = "Not Found";
const char* ERROR_404_FORM = "The Requested file was not found from this server";
const char* ERROR_500      = "Internal Error";
const char* ERROR_500_FORM = "There was an unusual problem serving the request file";

Client::Client(int epoll_fd,int sock_fd):socket_fd(sock_fd),epoll_fd(epoll_fd),last_event_happen(0){

}

Client::~Client(){
    closeConnection();
}

void Client::init(MysqlConnectionPool* p_sql_pool,Timer* p_timer,int64_t t_id,int _socket_fd,int _port){
    mysql_pool_ptr=p_sql_pool;
    timer_ptr=p_timer;
    timer_id=t_id;
    socket_fd=_socket_fd;
    port=_port;
    flushEventHapenTime();
    init();
}

void Client::init(){
    memset(read_buffer,0,READ_BUFFER_SIZE);
    memset(write_buffer,0,WRITE_BUFFER_SIZE);
    memset(file_name,0,FILE_NAME_SIZE);

    //读事件相关变量：
    read_pos=check_pos=line_start=content_len=0;
    url=host=version=content=NULL;
    cgi=keep_alive=false;
    check_state=CHECK_REQUEST_LINE;
    method=GET;

    //写事件相关变量：
    write_pos=byte_has_send=byte_need_send=iovec_count=0;
    file_addr=NULL;
    io_vec[0].iov_base=io_vec[1].iov_base=NULL;
    io_vec[0].iov_len=io_vec[1].iov_len=0;
    memset(&file_stat,0,sizeof(file_stat));
}

int Client::closeConnection(){
    if(socket_fd!=-1){
        epoll_remove(epoll_fd,socket_fd);
        close(socket_fd);
        Logger::Info("Client.closeConnection socket_fd=%d,port=%d",socket_fd,port);
        socket_fd=-1;
        close_mmap();
        timer_ptr->StopTimer(timer_id);
        timer_id=-1;
    }
    return -1;
}

void Client::flushEventHapenTime(){
    last_event_happen=now_sec()+Config::over_time;
}

bool Client::isOverTime(){
    return last_event_happen<now_sec();
}

//------------读取数据与解析报文部分

bool Client::getDataFromSocketBuffer(){
    if(read_pos>=READ_BUFFER_SIZE){
        return false;
    }
    while(true){
        int byteRead=recv(socket_fd,read_buffer+read_pos,READ_BUFFER_SIZE-read_pos,0);
        if(byteRead<=0){
            if(byteRead==0) return false;
            if(errno==EAGAIN || errno==EWOULDBLOCK){
                break;
            }
        }
        read_pos+=byteRead;
    }
    Logger::Info("Client.getDataFromSocketBuffer socket_fd=%d,read_bytes=%d",socket_fd,read_pos);
    return true;
}

LINE_STATE Client::praseLine(){
    char temp;
    for(;check_pos<read_pos;check_pos++){
        temp=read_buffer[check_pos];
        if(temp=='\r'){
            if(check_pos+1>=read_pos) return LINE_OPEN;
            if(read_buffer[check_pos+1]=='\n'){
                read_buffer[check_pos++]='\0';
                read_buffer[check_pos++]='\0';
                return LINE_OK;
            }else return LINE_BAD;
        }else if(temp=='\n'){
            if(check_pos&&read_buffer[check_pos-1]=='\r'){
                read_buffer[check_pos-1]='\0';
                read_buffer[check_pos++]='\0';
                return LINE_OK;
            }else return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HTTP_CODE Client::praseRequestLine(char* text){
    url=strpbrk(text," \t");
    if(!url) return BAD_REQUEST;
    *url='\0';url++;

    if(strcasecmp(text,"GET")==0){
        method=GET;
    }else if(strcasecmp(text,"POST")==0){
        method=POST;
        cgi=true;
    }else return BAD_REQUEST;

    url+=strspn(url," \t");
    version=strpbrk(url," \t");
    if(!version) return BAD_REQUEST;
    *version='\0';version++;
    version+=strspn(version," \t");

    if(strcasecmp(version,"HTTP/1.1")!=0) return BAD_REQUEST;

    if(strncasecmp(url,"http://",7)==0){
        url+=7;
    }else if(strncasecmp(url,"https://",8)==0){
        url+=8;
    }
    url=strchr(url,'/');

    if(!url||url[0]!='/') return BAD_REQUEST;

    return NO_REQUEST;
}

HTTP_CODE Client::praseHeader(char* text){
    if(*text=='\0'){
        if(content_len!=0){
            check_state=CHECK_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }else if(strncasecmp(text,"Connection:",11)==0){
        text+=11;
        text+=strspn(text," \t");
        if(strcasecmp(text,"Keep-Alive")==0){
            keep_alive=true;
        }
    }else if(strncasecmp(text,"Content-Length:",15)==0){
        text+=15;
        text+=strspn(text," \t");
        content_len=atoi(text);
    }else if(strncasecmp(text,"Host:",5)==0){
        text+=5;
        text+=strspn(text," \t");
        host=text;
    }
    return NO_REQUEST;
}

HTTP_CODE Client::praseContent(char* text){
    if(read_pos>=check_pos+content_len){
        text[content_len]='\0';
        content=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


HTTP_CODE Client::requestHandle(){
    strcpy(file_name,file_path_root.c_str());
    int len=strlen(file_name);
    const char* p=strrchr(url,'/');
    std::string temp(200,'\0');
    if(cgi && *(p+1)=='2'||*(p+1)=='3'){
        std::string name;
        std::string password;
        int i;

        for(i=5;content[i]!='&';i++){
            name.push_back(content[i]);
        }

        i+=10;
        for(;content[i]!='\0';i++){
            password.push_back(content[i]);
        }

        if(*(p+1)=='3'){
            AutoLock locker(&user_dict_lock);
            if(user_dict->find(name)==user_dict->end()){
                char sql_insert[150]={0};
                strcat(sql_insert,"INSERT INTO user(name,password) VALUE('");
                strcat(sql_insert,name.c_str());
                strcat(sql_insert,"','");
                strcat(sql_insert,password.c_str());
                strcat(sql_insert,"')");
                MysqlConnectionRAII mysql_connect(mysql_pool_ptr);
                int res=mysql_query(mysql_connect.real_ptr(),sql_insert);
                if(!res){
                    strcpy(url,Client::resource_name.login);
                    (*user_dict)[name]=password;
                }else{
                    perror(mysql_error(mysql_connect.real_ptr()));
                    strcpy(url,Client::resource_name.registerError);
                }
            }else{
                strcpy(url,Client::resource_name.registerError);
            }
        }else{
            AutoLock locker(&user_dict_lock);
            if(user_dict->find(name)!=user_dict->end()&&(*user_dict)[name]==password){
                strcpy(url,Client::resource_name.welcome);
            }else{
                strcpy(url,Client::resource_name.registerError);
            }
        }
    }else if(*(p+1)=='0'){
        strcpy(url,Client::resource_name.regist);
    }else if(*(p+1)=='1'){
        strcpy(url,Client::resource_name.login);
    }else if(*(p+1)=='5'){
        strcpy(url,Client::resource_name.picture);
    }else if(*(p+1)=='6'){
        strcpy(url,Client::resource_name.video);
    }else if(*(p+1)=='7'){
        strcpy(url,Client::resource_name.fans);
    }else if(strlen(url)==1){
        strcpy(url,Client::resource_name.judge);
    }

    strncpy(file_name+len,url,FILE_NAME_SIZE-len-1);

    if(stat(file_name,&file_stat)<0){
        return NO_REQUEST;
    }

    if(!(file_stat.st_mode & S_IROTH)){
        return FORBIDDEN_REQUEST;
    }else if(S_ISDIR(file_stat.st_mode)){
        return BAD_REQUEST;
    }

    int file_fd=open(file_name,O_RDONLY);
    file_addr=(char*)mmap(NULL,file_stat.st_size,PROT_READ,MAP_PRIVATE,file_fd,0);
    close(file_fd);
    return FILE_REQUEST;
}

HTTP_CODE Client::analysisHttpMessage(){
    HTTP_CODE http_code=NO_REQUEST;
    LINE_STATE line_state=LINE_OK;
    char* text=NULL;
    while((check_state==CHECK_CONTENT&&line_state==LINE_OK)||(line_state=praseLine())==LINE_OK){
        text=getLine();
        line_start=check_pos;
        switch (check_state){
            case CHECK_REQUEST_LINE:{
                http_code=praseRequestLine(text);
                if(http_code==BAD_REQUEST) return BAD_REQUEST;
                else if(http_code==NO_REQUEST){
                    check_state=CHECK_HEADER;
                }
                break;
            }
            case CHECK_HEADER:{
                http_code=praseHeader(text);
                if(http_code==GET_REQUEST){
                    return requestHandle();
                }
                break;
            }
            case CHECK_CONTENT:{
                http_code=praseContent(text);
                if(http_code==GET_REQUEST){
                    return requestHandle();
                }
                line_state=LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

void Client::readEventHandle(){
    bool is_ok=getDataFromSocketBuffer();
    if(!is_ok){
        closeConnection();
        return;
    }
    HTTP_CODE http_code=analysisHttpMessage();
    if(http_code==NO_REQUEST){
        epoll_mod(epoll_fd,socket_fd,EPOLLIN,true);
        Logger::Info("client.readEventHandle NO_REQUEST socket_fd=%d",socket_fd);
        return;
    }
    
    Logger::Info("Client.readEventHandle Analysis Http Message Done  socket_fd=%d,HTTP code=%d",socket_fd,http_code);

    is_ok=generalHttpMessage(http_code);
    if(!is_ok){
        Logger::Info("Client.readEventHandle  generalHttpMessage faild! socket_fd=%d",socket_fd);
        closeConnection();
        return;
    }
    epoll_mod(epoll_fd,socket_fd,EPOLLOUT,true);
}

bool Client::addRespone(const char*format,...){
    if(write_pos>=WRITE_BUFFER_SIZE){
        return false;
    }
    va_list values;
    va_start(values,format);
    bool is_ok=true;
    int len=vsnprintf(write_buffer+write_pos,WRITE_BUFFER_SIZE-write_pos,format,values);
    if(len>=WRITE_BUFFER_SIZE-1-write_pos){
        is_ok=false;
    }
    write_pos+=len;
    va_end(values);
    return is_ok;
}

bool Client::addStaueLine(int staue,const char* title){
    return addRespone("HTTP/1.1 %d %s\r\n",staue,title);
}

bool Client::addHeader(int content_len){
    return addContentLen(content_len)&&addContentType()&&addKeepAlive()&&addBlankLine();
}

bool Client::addContentLen(int content_len){
    return addRespone("Content-Length:%d\r\n",content_len);
}

bool Client::addContentType(){
    return addRespone("Content-Type:%s\r\n","text/html");
}

bool Client::addKeepAlive(){
    return addRespone("Connection:%s\r\n",keep_alive?"keep-alive":"close");
}

bool Client::addBlankLine(){
    return addRespone("%s","\r\n");
}

bool Client::addContent(const char* text){
    return addRespone("%s",text);
}

bool Client::generalHttpMessage(HTTP_CODE http_code){
    bool result=true;
    switch (http_code){
        case INTERNAL_ERROR:{
            result=addStaueLine(500,ERROR_500)&&
                    addHeader(strlen(ERROR_500_FORM))&&
                    addContent(ERROR_500_FORM);
            break;
        }
        case BAD_REQUEST:{
            result=addStaueLine(400,ERROR_400)&&
                    addHeader(strlen(ERROR_400_FORM))&&
                    addContent(ERROR_400_FORM);
            break;
        }
        case NO_RESOURCE:{
            result=addStaueLine(404,ERROR_404)&&
                    addHeader(strlen(ERROR_400))&&
                    addContent(ERROR_404);
            break;
        }
        case FORBIDDEN_REQUEST:{
            result=addStaueLine(403,ERROR_403)&&
                    addHeader(strlen(ERROR_403_FORM))&&
                    addContent(ERROR_403_FORM);
            break;
        }
        case FILE_REQUEST:{
            result=addStaueLine(200,OK_200);
            if(!result) break;
            if(file_stat.st_size!=0){
                if(!(result=addHeader(file_stat.st_size))) break;
                io_vec[0].iov_base=write_buffer;
                io_vec[0].iov_len=write_pos;
                io_vec[1].iov_base=file_addr;
                io_vec[1].iov_len=file_stat.st_size;
                byte_need_send=io_vec[0].iov_len+io_vec[1].iov_len;
                iovec_count=2;
                return true;
            }else{
                const char* text="<html><body></body></html>";
                result=addHeader(strlen(text))&&addContent(text);
            }
            break;
        }
        default:result=false;
    }
    io_vec[0].iov_base=write_buffer;
    io_vec[0].iov_len=write_pos;
    byte_need_send=io_vec[0].iov_len;
    iovec_count=1;
    return result;
}

bool Client::sendDataToSocketBuffer(){

    int byte_written=0;
    while (byte_need_send>0){
        byte_written=writev(socket_fd,io_vec,iovec_count);

        if(byte_written<0){
            Logger::Info("client.sendDataToSocketBuffer  writev faild errno=%d",errno);
            if(errno==EAGAIN){
                return true;
            }else{
                return false;
            }
        }

        byte_has_send+=byte_written;
        byte_need_send-=byte_written;
        if(byte_written>io_vec[0].iov_len){
            io_vec[0].iov_len=0;
            io_vec[1].iov_base=file_addr+byte_has_send-write_pos;
            io_vec[1].iov_len=byte_need_send;
        }else{
            io_vec[0].iov_base=write_buffer+byte_has_send;
            io_vec[0].iov_len-=byte_written;
        }

    }
    return true;
}

void Client::close_mmap(){
    if(file_addr){
        munmap(file_addr,file_stat.st_size);
        file_addr=NULL;
    }
}

void Client::writeEventHandle(){
    bool is_ok=sendDataToSocketBuffer();
    if(!is_ok){
        Logger::Info("Client.writeEventHandle  Send Data To Socket Buffer Faild! socket_fd=%d",socket_fd);
        close_mmap();
        closeConnection();
        return;
    }

    if(byte_need_send>0){
        Logger::Info("Client.writeEventHandle send not done!  socket_fd=%d,byte_need_send=%d",socket_fd,byte_need_send);
        epoll_mod(epoll_fd,socket_fd,EPOLLOUT,true);
    }else{
        Logger::Info("Client.writeEventHandle send data done! socket_fd=%d",socket_fd);
        close_mmap();
        if(keep_alive){
            epoll_mod(epoll_fd,socket_fd,EPOLLIN,true);
            init();
        }else{
            closeConnection();
        }
    }

}