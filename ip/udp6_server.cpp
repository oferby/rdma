// https://www.ietf.org/rfc/rfc3493.txt

#include <stdio.h>
#include<stdlib.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netdb.h>

int main()
{

	uint16_t port = 9898;

   	int sock;
   	int status;
   	sockaddr_in6 addr6 {0};
	sockaddr_in6 *client;

	addr6.sin6_family = AF_INET6;
	addr6.sin6_port = htons(port);
	addr6.sin6_addr = in6addr_any;
	// addr6.sin6_addr = in6addr_loopback;
	
	sock = socket(AF_INET6, SOCK_DGRAM, 0);

   int  hoplimit = 10;

   if (setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
                  (char *) &hoplimit, sizeof(hoplimit)) == -1)
       perror("setsockopt IPV6_UNICAST_HOPS");


	int on = 1;

	if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
					(char *)&on, sizeof(on)) == -1)
		perror("setsockopt IPV6_V6ONLY");
	else
		printf("IPV6_V6ONLY set\n");

// 	The functions that the application uses to pass addresses
//    into the system are:

//       bind()
//       connect()
//       sendmsg()
//       sendto()

// The functions that return an address from the system to an application
//    are:

//       accept()
//       recvfrom()
//       recvmsg()
//       getpeername()
//       getsockname()

	status = bind(sock, (struct sockaddr*) &addr6, sizeof addr6);
	if (status == -1) {
		perror("error creating socket");
		exit(1);
	}


	int if_indx = if_nametoindex("enp125s0f0");
	printf("int index: %i\n", if_indx);

	socklen_t clientaddr_len = sizeof(struct sockaddr_in6);

	char *buf = (char*) calloc (1, 2049);

	while (1)
	{
		auto data  = recvfrom(sock, buf, 2048, 0, (sockaddr*) client, &clientaddr_len);
		
		if (data < 0 ) {
			perror("error getting data");
		} else {
			printf("received %i bytes\n",data);
			printf("message: %s\n\n", buf);
		}


		
	}
		


}
