
#include <stdio.h>
#include "rdma_handler.h"
#include "udp_server.h"

void print_usage_and_exit() {
    puts("\nUSAGE: app <dev> <IP> [<-s> <IP>]\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    
    bool initiator = false;
    bool sent = false;

   
    if (argc < 3) 
        print_usage_and_exit();    

    // ConnectionServer conn_server {argv[2]};    
    RdmaHandler rdmaHandler {argv[1]};

    // conn_server.set_rdma_handler(&rdmaHandler);

    // conn_server.start();

    // if (argc > 3 && strcmp(argv[3],"-s") == 0) {
    //     if (argc == 5) {
    //         conn_server.send_hello(argv[4]);
    //         initiator = true;
    //     } else {
    //         print_usage_and_exit();
            
    //     }
        
    // }

    // while (1)
    // {
    //     conn_server.handle_events();
    //     rdmaHandler.poll_complition();

    //     if (!sent & initiator & rdmaHandler.is_ready() ) {
            
    //         puts("sending SR.");
    //         neighbor *n = conn_server.get_app_dest();
    //         std::string data = "this is a message!";
    //         rdmaHandler.send_to_dest(data.c_str(), data.length());

    //         sent = true;

    //     }

    // }

}
