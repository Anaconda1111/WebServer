
#include "server/server.h"

int main(){
    int port=9996;
    Server web_server(port);
    web_server.init();
    web_server.startListen();
    web_server.startEpollLoop();
    return 0;    
}