#include <stdio.h>
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

using namespace std;

#define MAX_EVENTS 10
#define PORT 8585


class ConnectionServer {

private:

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

        rem_dest->gid = (ibv_gid*) malloc(sizeof rem_dest->gid);
        wire_gid_to_gid(tmp_gid, rem_dest->gid);

    }

    
    void add_neighbor(sockaddr_in *clientaddr, socklen_t *clientaddr_len, app_dest *rem_dest) {

        char *ip = inet_ntoa(clientaddr->sin_addr);
  
        if (neighbor_map.count(clientaddr->sin_addr.s_addr)  == 0 ) {
            puts("adding new addr");
            
            neighbor n = {
                .addr = clientaddr,
                .dest = rem_dest,
                .lastHello = time(nullptr)
            };

            neighbor_map[clientaddr->sin_addr.s_addr] = n;

            app_context *ctx = rdmaHandler->get_app_context();

            ibv_ah_attr ah_attr = {
                .dlid = rem_dest->lid,
                .sl = 0,
                .src_path_bits = 0,
                .port_num = PORT_NUM
            };
            
            ah_attr.is_global = 1;
            ah_attr.grh.dgid = *(rem_dest->gid);
            ah_attr.grh.hop_limit = 1;
            ah_attr.grh.sgid_index = GID_IDX;

            ibv_ah *ah = ibv_create_ah(ctx->pd, &ah_attr);
            if (!ah) {
                perror("error creating AH.");
            }

            n.dest->ah = ah;

            check_sending_hello(clientaddr, clientaddr_len);
        
        }
            
        else {
            
            printf("address %s exists. updating last hello time.\n", ip);
            neighbor_map[clientaddr->sin_addr.s_addr].lastHello = time(nullptr);
            free(rem_dest->gid);
            free(rem_dest);

            check_sending_hello(clientaddr, clientaddr_len);

        }

    }

    void check_sending_hello(sockaddr_in *clientaddr, socklen_t *clientaddr_len) {
        
        if (pending_hello.find(clientaddr->sin_addr.s_addr) == pending_hello.end() ) {
            
            puts("client address not in pending list. sending hello.");

            pending_hello.insert(clientaddr->sin_addr.s_addr);

            send_hello(clientaddr, clientaddr_len);

            return;
        } 

        puts("client address in pending list.");
        pending_hello.erase(clientaddr->sin_addr.s_addr);

    }

    void set_hello_msg(app_dest *dest) {

        local_dest = dest;
        this->hello_msg = get_hello_msg(dest);
        this->msg_size = sizeof "0000:000000:000000:00000000000000000000000000000000";
    }


public:

    void set_rdma_handler(RdmaHandler *handler) {
        rdmaHandler = handler;
        app_dest *dest =  rdmaHandler->get_local_dest();
        set_hello_msg(dest);
    }

    void start() {

        sd = socket(AF_INET, SOCK_DGRAM, 0);

        if ( sd == -1 ) {
            fprintf(stderr, "Could not create a socket!\n");
            exit(1);
        }

        printf("Socket created!\n");
    
        struct sockaddr_in simpleServer;
        auto addresslen = sizeof(simpleServer);
        bzero(&simpleServer,addresslen);
        simpleServer.sin_family = AF_INET;
        simpleServer.sin_addr.s_addr = INADDR_ANY;
        simpleServer.sin_port = htons(PORT);
    
        int status = bind(sd, (struct sockaddr *) &simpleServer, addresslen);
        if ( status != 0 ) {
            fprintf(stderr, "Could not bind socket!\n");
            exit(1);    
        }

        puts("Socket binded!");


        // create epoll fd
        epollfd = epoll_create1(0);
        if(epollfd<0) {
            puts("error creating epoll fd.");
            exit(1);

        }

        ev.events = EPOLLIN;
        ev.data.fd = sd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sd, &ev) == -1 ) {
            perror("could not add listen socket to epoll.");
            exit(EXIT_FAILURE);
        }

        puts("listen socket added to epoll list.");


    };

    void server_receive_event()
    {
        char recvbuf[65536] = { 0 }; 
        int len;
        sockaddr_in clientaddr;
        socklen_t clientaddr_len = sizeof(struct sockaddr);

        len = recvfrom(sd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&clientaddr, &clientaddr_len);
        if (len > 0) {
            char *ip = inet_ntoa(clientaddr.sin_addr);
            printf("got %i bytes from %s\n", len, ip);

            app_dest *rem_dest;
            rem_dest = (app_dest*) malloc(sizeof rem_dest);
            memset(rem_dest, 0, sizeof rem_dest);
            
            get_dest(recvbuf, rem_dest);
            
            print_dest(rem_dest);
            add_neighbor(&clientaddr, &clientaddr_len, rem_dest);
        }
    }

    void handle_events() {

        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 100);
        if (nfds == -1) {
            perror("epoll wait error!");
            exit(EXIT_FAILURE);
        } else if (nfds > 0) {
            puts("got socket event.");
            server_receive_event();
        }
        

    };

    void send_hello(char *dest) {

        printf("sending first hello message to %s\n", dest);

        sockaddr_in servaddr;
        bzero(&servaddr,sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(dest);
        servaddr.sin_port = htons(PORT);

        this->pending_hello.insert(servaddr.sin_addr.s_addr);

        socklen_t clientaddr_len = sizeof(struct sockaddr);
        send_hello(&servaddr, &clientaddr_len);


    }

    void send_hello(sockaddr_in *dest, socklen_t *clientaddr_len) {
    
        printf("sending hello to %s\n", inet_ntoa(dest->sin_addr));

        int result;

        result = sendto(sd, this->hello_msg, this->msg_size, 0, (sockaddr*) dest, *clientaddr_len);
        if (result < 0) {
            perror("error sendin hellow message");
        } else {
            printf("%i bytes sent.\n", result);
        }
    }



    neighbor* get_app_dest(const char *addr) {
         
        // in_addr *dest_addr = new in_addr();

        // inet_aton(addr, dest_addr);
        
        // if (neighbor_map[dest_addr->s_addr].addr == nullptr)
        //     return nullptr;
        
        // return &neighbor_map[dest_addr->s_addr];


        if(neighbor_map.begin()->second.addr == nullptr)
            return nullptr;

        return &neighbor_map.begin()->second;

    }

    neighbor* get_app_dest() {
         
        if(neighbor_map.begin()->second.addr == nullptr)
            return nullptr;

        return &neighbor_map.begin()->second;

    }


};
