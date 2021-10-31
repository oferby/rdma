#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>

void ping(int s, const char *message)
{
    char buf[8192];

    strncpy(buf, message, sizeof(buf));
    send(s, buf, strlen(buf), 0);
    recv(s, buf, 8192, 0);
    strtok(buf, "\n");
    puts(buf);
}

int main()
{
    int s;
    struct sockaddr_in6 addr;

    s = socket(AF_INET6, SOCK_STREAM, 0);
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(5000);
    inet_pton(AF_INET6, "::1", &addr.sin6_addr);
    // inet_pton(AF_INET6, "fd6f:573a:8a29::1", &addr.sin6_addr);
    // inet_pton(AF_INET6, "fe80::aa49:4dff:feed:d552", &addr.sin6_addr);
    connect(s, (struct sockaddr *)&addr, sizeof(addr));

    std::string s1 = "msg1\n";
    ping(s, s1.c_str() );
    std::string s2 = "msg2\n";
    ping(s, s2.c_str() );
    std::string s3 = "msg3\n";
    ping(s, s3.c_str() );

    return 0;
}