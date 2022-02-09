#include "udp_server.h"

using namespace std;


ConnectionServer::ConnectionServer(char *ip_addr) {
    local_addr = {ip_addr};
}

void ConnectionServer::set_rdma_handler(RdmaHandler *handler) {
            rdmaHandler = handler;
            app_dest *dest =  rdmaHandler->get_local_dest();
            set_hello_msg(dest);
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

void ConnectionServer::server_receive_event()
        {
            char recvbuf[65536] = { 0 }; 
            int len;
            sockaddr_in clientaddr;
            socklen_t clientaddr_len = sizeof(struct sockaddr);

            len = recvfrom(sd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&clientaddr, &clientaddr_len);
            if (len > 0) {

                char *ip = inet_ntoa(clientaddr.sin_addr);
                printf("got %i bytes from %s\n", len, ip);

                app_dest *rem_dest = new app_dest;
                get_dest(recvbuf, rem_dest);
                print_dest(rem_dest);
                add_neighbor(&clientaddr, &clientaddr_len, rem_dest);
            }
        }

void ConnectionServer::handle_events() {

            nfds = epoll_wait(epollfd, events, MAX_EVENTS, 10);
            if (nfds == -1) {
                perror("epoll wait error!");
                exit(EXIT_FAILURE);
            } else if (nfds > 0) {
                puts("got socket event.");
                server_receive_event();
            }
            

        };

void ConnectionServer::send_hello(char *dest) {

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

void ConnectionServer::send_hello(sockaddr_in *dest, socklen_t *clientaddr_len) {
            
            // printf("message: %s, size %i\n", this->hello_msg, this->msg_size);
            printf("sending hello to %s\n", inet_ntoa(dest->sin_addr));

            
            int result;
            result = sendto(sd, this->hello_msg, this->msg_size, 0, (sockaddr*) dest, *clientaddr_len);
            if (result < 0) {
                perror("error sending hellow message");
            } else {
                printf("%i bytes sent.\n", result);
            }
        }

neighbor* ConnectionServer::get_app_dest(const char *addr) {

    if(neighbor_map.empty())
        return nullptr;

    return &neighbor_map.begin()->second;

}

neighbor* ConnectionServer::get_app_dest() {
    
    if(neighbor_map.begin()->second.addr == nullptr)
        return nullptr;

    return &neighbor_map.begin()->second;

}

void ConnectionServer::set_hello_msg(app_dest *dest) {

    local_dest = dest;
    this->hello_msg = get_hello_msg(dest);
    this->msg_size = sizeof "0000:000000:000000:00000000000000000000000000000000";
}

void ConnectionServer::add_neighbor(sockaddr_in *clientaddr, socklen_t *clientaddr_len, app_dest *rem_dest) {
    
    if (neighbor_map.count(clientaddr->sin_addr.s_addr)  == 0 ) {
        
        puts("adding new addr");
        
        neighbor n = {
            .addr = clientaddr,
            .dest = rem_dest,
            .lastHello = time(nullptr)
        };

        neighbor_map[clientaddr->sin_addr.s_addr] = n;

        rdmaHandler->set_dest(rem_dest);

        puts("remote address added.");

        check_sending_hello(clientaddr, clientaddr_len);
    
    } else {

        char *ip = inet_ntoa(clientaddr->sin_addr);

        printf("address %s exists. updating last hello time.\n", ip);
        neighbor_map[clientaddr->sin_addr.s_addr].lastHello = time(nullptr);
        free(rem_dest->gid);
        free(rem_dest);

    }

}

void ConnectionServer::check_sending_hello(sockaddr_in *clientaddr, socklen_t *clientaddr_len) {
    
    if (pending_hello.find(clientaddr->sin_addr.s_addr) == pending_hello.end() ) {
        
        puts("client address not in pending list. sending hello.");

        pending_hello.insert(clientaddr->sin_addr.s_addr);

        send_hello(clientaddr, clientaddr_len);

        return;
    } 

    puts("client address in pending list.");
    pending_hello.erase(clientaddr->sin_addr.s_addr);

}






