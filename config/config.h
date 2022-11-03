#ifndef CONFIG_H__
#define CONFIG_H__

#include<string>

namespace Config{
    const std::string mysql_user     ="root";
    const std::string mysql_password ="123456";
    const std::string mysql_dbname   ="WebServer";
    const std::string mysql_addr     ="localhost";
    const int         mysql_port     =9996;
    const int         mysql_max_conn =20;

    const int thread_pool_max_thread =20;
    
    const time_t over_time=20;
    const int alarm_time=5;

    const std::string log_file_path ="/mnt/hgfs/share/WebServer/log.txt";

    const unsigned int client_timeout_check_nums=20;
    const unsigned int client_timeout_check_continue_threshold=4;
};


#endif