#ifndef UDP_SERVER

#define UDP_SERVER 

#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <set>
#include <map>
#include "rdma_handler.h"

#define MAX_EVENTS 10
#define PORT 8585

#define MSG_FORMAT "%04x:%06x:%06x:%06x:%06x:%32s"
#define MSG_SIZE   66
#define MSG_SSCAN  "%x:%x:%x:%x:%x:%s"

#define TERMINATION_FORMAT "%s"
#define TERMINATION_MSG_SIZE 4
#define TERMINATION_MSG "END"
static int page_size;
static int use_odp;


class ConnectionServer {

    private:

        std::string local_addr;
        RdmaHandler *rdmaHandler;

        std::map <uint32_t, neighbor> neighbor_map;
        std::set <uint32_t> pending_hello;

        int nfds, epollfd, status, sd;
        struct epoll_event ev, events[MAX_EVENTS];

        char* get_hello_msg(app_dest *dest); 
        app_dest* get_dest(char *msg);
        void add_neighbor(sockaddr_in *clientaddr, socklen_t *clientaddr_len, app_dest *rem_dest);
        void check_sending_hello(sockaddr_in *clientaddr, socklen_t *clientaddr_len);
        void set_hello_msg(app_dest *dest);


    public:

        ConnectionServer(char *ip_addr);
        void set_rdma_handler(RdmaHandler *handler); 
        void start(); 
        void poll_events();
        void server_receive_event();
        // void send_hello(char *dest); 
        // void send_hello(sockaddr_in *dest, socklen_t *clientaddr_len); 
        // neighbor* get_app_dest(const char *addr); 
        // neighbor* get_app_dest(); 

};


#endif