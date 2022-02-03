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


class ConnectionServer {

    private:

        std::string local_addr;
        RdmaHandler *rdmaHandler;

        std::map <uint32_t, neighbor> neighbor_map;
        std::set <uint32_t> pending_hello;

        int nfds, epollfd, status, sd;
        struct epoll_event ev, events[MAX_EVENTS];

        app_dest *local_dest;    
        char *hello_msg;
        ssize_t msg_size;

        static char* get_hello_msg(app_dest *dest) {

            char *msg = (char*) malloc(sizeof "0000:000000:000000:00000000000000000000000000000000");

            char gid[33];
            gid_to_wire_gid(dest->gid, gid);
            sprintf(msg, "%04x:%06x:%06x:%s", dest->lid, dest->qpn,
                            dest->psn, gid);
            return msg;

        }

        static void get_dest(char *msg, app_dest *rem_dest) {

            char tmp_gid[33];
            sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn,
                                    &rem_dest->psn, tmp_gid);

            rem_dest->gid = new ibv_gid;
            wire_gid_to_gid(tmp_gid, rem_dest->gid);

        }
        
        void add_neighbor(sockaddr_in *clientaddr, socklen_t *clientaddr_len, app_dest *rem_dest);

        void check_sending_hello(sockaddr_in *clientaddr, socklen_t *clientaddr_len);

        void set_hello_msg(app_dest *dest);


    public:

        ConnectionServer(char *ip_addr);
        void set_rdma_handler(RdmaHandler *handler); 
        void start(); 
        void server_receive_event();
        void handle_events(); 
        void send_hello(char *dest); 
        void send_hello(sockaddr_in *dest, socklen_t *clientaddr_len); 
        neighbor* get_app_dest(const char *addr); 
        neighbor* get_app_dest(); 

};


#endif