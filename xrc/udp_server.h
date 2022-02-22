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
        RdmaHandler* rdmaHandler;

        int nfds, epollfd, listen_sock, status;
        std::set <int> pending_hello;
        
        epoll_event events[MAX_EVENTS];
        
        void server_receive_event(int sd);
        void send_hello(int sd, char* hello_msg); 
        void recv_hello(int sd);
        char* get_hello_msg(sockaddr_in* clientaddr, char* msg); 

    public:

        ConnectionServer(char* ip_addr);
        void send_hello(char* dest);
        void set_rdma_handler(RdmaHandler* handler); 
        void start(); 
        void poll_events();

};




static void epoll_ctl_del(int epfd, int sd) {
    if (epoll_ctl(epfd, EPOLL_CTL_DEL,
                    sd, NULL) == -1 ) {
        perror("error deleting from epoll");
        exit(EXIT_FAILURE);
    }

}


static void epoll_ctl_add(int epfd, int fd, uint32_t events)
{
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		perror("epoll_ctl()\n");
		exit(1);
	}
}

static int setnonblocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFD, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) ==
	    -1) {
		return -1;
	}
	return 0;
}
#endif