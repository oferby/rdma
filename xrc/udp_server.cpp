#include "udp_server.h"

ConnectionServer::ConnectionServer(char *ip_addr) {

    local_addr = {ip_addr};
}

void ConnectionServer::set_rdma_handler(RdmaHandler *handler) {
    rdmaHandler = handler;
}

void ConnectionServer::start() {

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    if ( listen_sock == -1 ) {
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

    int status = bind(listen_sock, (struct sockaddr *) &simpleServer, addresslen);
    if ( status != 0 ) {
        fprintf(stderr, "Could not bind socket!\n");
        exit(1);    
    }

    puts("Socket binded!");

    status = listen(listen_sock, 5);
    if ( status != 0 ) {
        fprintf(stderr, "Could not listen on socket!\n");
        exit(1);    
    }

    puts("listen on socket.");

    // create epoll fd
    epollfd = epoll_create1(0);
    if(epollfd<0) {
        puts("error creating epoll fd.");
        exit(1);

    }

    epoll_ctl_add(epollfd, listen_sock, EPOLLIN);

    puts("listen socket added to epoll list.");

};

void ConnectionServer::poll_events() {

    nfds = epoll_wait(epollfd, events, MAX_EVENTS, 10);
    if (nfds == -1) {
        perror("epoll wait error!");
        exit(EXIT_FAILURE);
    } else if (nfds > 0) {
        puts("got socket event.");
        server_receive_event(nfds);
    }

};


void ConnectionServer::server_receive_event(int nfds) {

    sockaddr_in cli_addr;
	int conn_sock;
	socklen_t socklen = sizeof(cli_addr);
	
    
    for (int i = 0; i < nfds; i++) {

        printf("[++] event %i: %i\n",i, events[i].events); 
        
        if (events[i].data.fd == listen_sock) {
				/* handle new connection */

				conn_sock =
				    accept(listen_sock,
					   (struct sockaddr *)&cli_addr,
					   &socklen);
                
                char buf[16];

				inet_ntop(AF_INET, (char *)&(cli_addr.sin_addr),
					  buf, sizeof(cli_addr));

				printf("[+] connected with %s:%d\n", buf,
				       ntohs(cli_addr.sin_port));

				setnonblocking(conn_sock);

				epoll_ctl_add(epollfd, conn_sock,
					      EPOLLIN | EPOLLET | EPOLLRDHUP |
					      EPOLLHUP);
                
        } else if (events[i].events & EPOLLRDHUP ) {
            puts("connection closed by other side.");
            close_socket(events[i].data.fd);

        } else if (events[i].events & EPOLLIN) {
            /* handle EPOLLIN event */    
            recv_hello(events[i].data.fd);

        } else {
            printf("[+] unexpected: %i\n", events[i].events);
        }
    
    }

};

void ConnectionServer::send_hello(char *dest) {

    printf("sending first hello message to %s\n", dest);

    int result;

    sockaddr_in servaddr;
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(dest);
    servaddr.sin_port = htons(PORT);
    socklen_t sock_len = sizeof(struct sockaddr);

    int sd = socket(AF_INET, SOCK_STREAM, 0);

    result = connect(sd, (sockaddr*) &servaddr, sock_len);
    if (result == -1) {
        printf("could not connect to %s", dest);
        exit(EXIT_FAILURE);
    }

    setnonblocking(sd);
    epoll_ctl_add(epollfd, sd, EPOLLIN | EPOLLET);
    puts("listen socket added to epoll list.");

    char* hello_msg = rdmaHandler->get_hello_msg(&servaddr.sin_addr.s_addr, nullptr);
    printf("sending hello to %s\n", dest);
    send_hello(sd, hello_msg);

}

void ConnectionServer::send_hello(int sd, char* hello_msg) {
    
    status = send(sd, hello_msg, MSG_SIZE, 0);
    if (status < 0) {
        perror("error sendin hellow message");
    } else {
        printf("%i bytes sent.\n", status);
        pending_hello.insert(sd);
    }

}


void ConnectionServer::recv_hello(int sd) {

    char* buf = new char[MSG_SIZE];

    int result = recv(sd, buf, MSG_SIZE, 0);
    if (result < 0) {
        perror("could not receive hello message.");
        exit(EXIT_FAILURE);
    }

    sockaddr_in* addr = new sockaddr_in();
    socklen_t sock_len = sizeof(sockaddr);
    status = getpeername(sd, (sockaddr*) addr, &sock_len);
    if (status == -1) {
        puts("error reading source address from socket. not stopping ...");
    }

    char* source = inet_ntoa(addr->sin_addr);
    printf("receveived %i bytes from %s\n",result, source);

    if (pending_hello.find(sd) == pending_hello.end()) {
        puts("socket not in pending list. sending hello message");
        
        char* msg = rdmaHandler->get_hello_msg_response(&addr->sin_addr.s_addr, buf);
        send_hello(sd, msg);

    } else {

        puts("socket in pending list. closing connection.");
        close_socket(sd);

    }

}

void ConnectionServer::close_socket(int sd) {

    pending_hello.erase(sd);
    epoll_ctl_del(epollfd, sd);
    close(sd);

}

