#include <stdio.h>
#include "rdma_handler.cpp"
#include "udp_server.cpp"

int main(int argc, char* argv[]) {
    
    bool initiator = false;
    bool sent = false;
    int cycles = 0;

    ConnectionServer conn_server;    
    RdmaHandler rdmaHandler;

    conn_server.set_rdma_handler(&rdmaHandler);


    conn_server.start();
   

    if (argc > 1) {
        conn_server.send_hello(argv[1]);
        initiator = true;
    }

    while (1)
    {
        conn_server.handle_events();
        rdmaHandler.poll_complition();

        if (initiator & cycles < 101 ) {
            ++cycles;
        }

        if (initiator & !sent & conn_server.get_app_dest() != nullptr ) {
            
            puts("sending SR.");
            neighbor *n = conn_server.get_app_dest();
            string data = "this is a message!";
            rdmaHandler.create_send_request(data.c_str(), sizeof data, n->dest);

            sent = true;

        }

    }
        

}
