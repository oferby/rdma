#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdexcept>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <malloc.h>
#include <getopt.h>
#include <time.h>
#include <arpa/inet.h>
#include <map>
#include <vector>

#include "rdma_handler.h"

using namespace std;


RdmaHandler::RdmaHandler() {
        
        memset(&app_ctx, 0, sizeof app_ctx);
        app_ctx.sge_map = &sge_map;
        app_ctx.available_send_sge_vector = &available_send_sge_vector;
        setup_context(&app_ctx);
        
        int status;
        
        local_dest = (app_dest*) malloc(sizeof local_dest);
        memset(local_dest, 0, sizeof local_dest);
        local_dest->lid = app_ctx.portinfo->lid;
        local_dest->qpn = app_ctx.qp->qp_num;
        local_dest->psn = 1;
        
        local_dest->gid = (ibv_gid*) malloc(sizeof local_dest->gid);

        status = ibv_query_gid(app_ctx.ctx, IB_PORT, GID_IDX, local_dest->gid);
        if (status == -1) {
            perror("could not get GID");
            exit(EXIT_FAILURE);
        }

        print_dest(local_dest);
        

}

app_dest* RdmaHandler::get_local_dest() {
    return local_dest;
}
    
app_context* RdmaHandler::get_app_context() {
    return &app_ctx;
}

void RdmaHandler::poll_complition() {

        status = ibv_poll_cq(app_ctx.cq, 1, &wc);
        if (status < 0) {
            perror("error getting WC.");
            return;

        }

        if( status > 0 && wc.status == ibv_wc_status::IBV_WC_SUCCESS) 
            handle_wc();

}


void RdmaHandler::create_send_request(const char *data, size_t len, app_dest *dest) {

    if (available_send_sge_vector.empty()) {
        puts("there is no SGE available to SR.");
        return;
    }

    puts("creating send request");

    ibv_sge *sge = available_send_sge_vector.back();
    available_send_sge_vector.pop_back();

    ibv_send_wr *send_wr = new ibv_send_wr();
    send_wr->wr_id = app_ctx.wid++,
    send_wr->sg_list = sge;
    send_wr->num_sge = 1;
    send_wr->opcode = IBV_WR_SEND;
    send_wr->wr.ud.ah = dest->ah;

    ibv_send_wr *bad_wr;

    // status = ibv_post_send(app_ctx.qp, send_wr, &bad_wr);

}



