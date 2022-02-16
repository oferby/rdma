#include "udp_server.h"

ConnectionServer::ConnectionServer(char *ip_addr) {

    local_addr = {ip_addr};
}

void ConnectionServer::set_rdma_handler(RdmaHandler *handler) {
    rdmaHandler = handler;
}

void ConnectionServer::start() {

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
    simpleServer.sin_addr.s_addr = inet_addr(local_addr.c_str());    //INADDR_ANY;
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

void ConnectionServer::poll_events() {

    nfds = epoll_wait(epollfd, events, MAX_EVENTS, 10);
    if (nfds == -1) {
        perror("epoll wait error!");
        exit(EXIT_FAILURE);
    } else if (nfds > 0) {
        puts("got socket event.");
        server_receive_event();
    }
    

};


void ConnectionServer::server_receive_event() {


}