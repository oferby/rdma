#include <stdio.h>
#include<stdlib.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>

int main(int argc, char *argv[])
{

    if (argc < 2) {
        puts("USAGE: udp6_client <IPv6>");
        exit(EXIT_FAILURE);
    }

	uint16_t port = 4791;

   	int sock;
   	int status;
   	sockaddr_in6 addr6 {0};
	
	sock = socket(AF_INET6, SOCK_DGRAM, 0);

    int on = 1;
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
					(char *)&on, sizeof(on)) == -1)
		perror("setsockopt IPV6_V6ONLY");
	else
		printf("IPV6_V6ONLY set\n");

    

    addr6 = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(port),
        .sin6_flowinfo = 0
    };

    // status = inet_pton(AF_INET6, "fe80::aa49:4dff:fe37:8c20", &addr6.sin6_addr);
    status = inet_pton(AF_INET6, argv[1], &addr6.sin6_addr);
    if (status != 1) {
        perror("error setting IPv6 address");
        exit(EXIT_FAILURE);
    }

    std::string msg = "this is a message!";

    status = sendto(sock, msg.c_str(), msg.length(), 0, (sockaddr*) &addr6, sizeof addr6);
    if (status < 0) {
        perror("could not send message");
        exit(EXIT_FAILURE);
    } else {
        printf("sent %i bytes\n\n", status);
    }
    


}