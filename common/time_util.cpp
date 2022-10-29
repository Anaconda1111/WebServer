#include "time_util.h"

time_t now_sec(){
    return time(NULL);
}

tm get_specific_time(){
    tm _tm;
    time_t now=now_sec();
    _tm=*localtime(&now);
    return _tm;
}