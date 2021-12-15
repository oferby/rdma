#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdexcept>
#include <string.h>

void create_send_request(const char *data, size_t len) {
    
    printf("data: %s\n" , data);

    char *d = (char*) malloc(len);

    memcpy(d, data, len);

    printf("D: %s\n", d);


}



int main(int argc, char* argv[]) {

    ibv_sge *sge = new ibv_sge();

    uint64_t addr = reinterpret_cast<uint64_t>(sge);

    printf("ADDR: %i\n", addr);
    
    ibv_sge *sge2 = reinterpret_cast<ibv_sge*>(addr);

    
    ibv_send_wr *send_wr = new ibv_send_wr();
    send_wr->wr_id = reinterpret_cast<uint64_t>(sge);

    printf("WR ID: %i\n", send_wr->wr_id);



    std::string data = "this is a message!";

    size_t l = data.length();

    create_send_request(data.c_str(), l);

    puts("done");

}