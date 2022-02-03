#include "rdma_handler.h"

using namespace std;


RdmaHandler::RdmaHandler(char *devname) {
        
        app_ctx = new app_context();
        app_ctx->sge_map = &sge_map;
        app_ctx->available_send_sge_vector = &available_send_sge_vector;
        app_ctx->devname = devname;
        
        setup_context();
        create_srq();
        setup_memory();
        create_queue_pair();
        create_local_dest();

        
}

void RdmaHandler::setup_context() {

    ibv_device   **dev_list, **tmp_dev_list;
    ibv_device   *ib_dev;
    int status;


    dev_list = ibv_get_device_list(NULL);
    
    if (!dev_list) {
        perror("error getting IB device list");
        exit(EXIT_FAILURE);
    }
    
    ib_dev = *dev_list;
    if (!ib_dev) {
        perror("device list empty");
        exit(EXIT_FAILURE);
    }
    
    tmp_dev_list = dev_list;
    bool found = false;
    while (tmp_dev_list)
    {
        if (strcmp(ib_dev->name, app_ctx->devname) == 0) {
            found = true;
            break;
        }
        tmp_dev_list++;
        ib_dev = *tmp_dev_list;
    }

    if (!found){
        printf("could not find dev %s", app_ctx->devname);
        exit(EXIT_FAILURE);
    }
    
    printf("using dev name: %s\n", ib_dev->name);

    app_ctx->ctx = ibv_open_device(ib_dev);
    if (!app_ctx->ctx) {
        perror("error creating context");
        exit(EXIT_FAILURE);
    }

    ibv_free_device_list(dev_list);

    ibv_port_attr port_info = {};
    app_ctx->portinfo = &port_info;
    status = ibv_query_port(app_ctx->ctx, IB_PORT, app_ctx->portinfo);
    if (status == -1) {
        perror("could not get port info");
        exit(EXIT_FAILURE);
    }

    app_ctx->pd = ibv_alloc_pd(app_ctx->ctx);
    if (!app_ctx->pd) {
	    fprintf(stderr, "Error, ibv_alloc_pd() failed\n");
	    exit(EXIT_FAILURE);
    }

    app_ctx->cq = ibv_create_cq(app_ctx->ctx, CQ_SIZE, nullptr, nullptr, 0);
    if (!app_ctx->cq) {
        perror("could not create CQ");
        exit(EXIT_FAILURE);
    }

}

void RdmaHandler::create_srq() {
    
    ibv_srq_init_attr attr = {
        .attr = {
            .max_wr = MAX_WR,
            .max_sge = MAX_SGE
        }
    };

    app_ctx->srq = ibv_create_srq(app_ctx->pd, &attr);
    if (!app_ctx->srq) {
        perror("error creating SRQ");
        exit(EXIT_FAILURE);
    }    


}

void RdmaHandler::setup_memory() {
    
    puts("setting up memory.");

    int mr_size =  ( MSG_SIZE + GRH_SIZE ) * MAX_WR;
    int alignment = sysconf(_SC_PAGESIZE);
    app_ctx->buf = (char*) memalign(alignment, mr_size);

    if (!app_ctx->buf) {
        perror("error creating memory buffer.");
        cleanup();
        exit(EXIT_FAILURE);
    }

    app_ctx->mr = ibv_reg_mr(app_ctx->pd, app_ctx->buf, mr_size, IBV_ACCESS_LOCAL_WRITE);
    if (!app_ctx->mr) {
        perror("error registering memory");
        cleanup();
        exit(EXIT_FAILURE);
    }

    uint32_t msg_size = MSG_SIZE + GRH_SIZE;

    uint64_t mem_addr = reinterpret_cast<uint64_t>(app_ctx->buf);

    ibv_sge *sge = (ibv_sge*) calloc(1, sizeof sge);
    sge->addr = mem_addr;
    sge->length = 0;
    sge->lkey = app_ctx->mr->lkey;

    app_ctx->available_send_sge_vector->push_back(sge);

    for (int i = 0; i < 2; i++) {

        mem_addr += msg_size;

        sge = (ibv_sge*) calloc(1, sizeof sge);
        sge->addr = mem_addr;
        sge->length = msg_size;
        sge->lkey = app_ctx->mr->lkey;

        ibv_recv_wr rec_wr = {};
        rec_wr.wr_id = reinterpret_cast<uint64_t>(sge);
        rec_wr.sg_list = sge;
        rec_wr.num_sge = 1;

        ibv_recv_wr *bad_wr;

        if (ibv_post_srq_recv(app_ctx->srq, &rec_wr, &bad_wr)) {
            perror("error posting RR in SRQ.");
            cleanup();
            exit(EXIT_FAILURE);    
        } 

    }

    
    puts("memory and WRs added.");

}

void RdmaHandler::create_queue_pair() {

    ibv_qp_init_attr_ex init_attr_ex = {0};
    init_attr_ex.send_cq = app_ctx->cq;
    init_attr_ex.recv_cq = app_ctx->cq;
    init_attr_ex.srq = app_ctx->srq;

    init_attr_ex.cap     = {};
    init_attr_ex.cap.max_send_wr  = MAX_WR;
    init_attr_ex.cap.max_recv_wr  = MAX_WR;
    init_attr_ex.cap.max_send_sge = MAX_SGE;
    init_attr_ex.cap.max_recv_sge = MAX_SGE;
            
    init_attr_ex.qp_type = IBV_QPT_RC;
    init_attr_ex.sq_sig_all = 1;
    init_attr_ex.pd = app_ctx->pd;
    init_attr_ex.comp_mask  = IBV_QP_INIT_ATTR_PD;

    app_ctx->qp = ibv_create_qp_ex(app_ctx->ctx, &init_attr_ex);
    if (!app_ctx->qp) {
        perror("could not create QP");
        exit(1);
    }

    puts("QP created.");

}

void RdmaHandler::create_local_dest() {
    
    local_dest = (app_dest*) calloc(1, sizeof local_dest);
    local_dest->lid = app_ctx->portinfo->lid;
    local_dest->qpn = app_ctx->qp->qp_num;
    local_dest->psn = 1;
    
    local_dest->gid = (ibv_gid*) calloc(1, sizeof local_dest->gid);

    status = ibv_query_gid(app_ctx->ctx, IB_PORT, GID_IDX, local_dest->gid);
    if (status == -1) {
        perror("could not get GID");
        exit(EXIT_FAILURE);
    }

    print_dest(local_dest);
}

app_dest* RdmaHandler::get_local_dest() {
    return local_dest;
}


// void RdmaHandler::do_qp_change(ibv_qp* qp, ibv_qp_attr *attr, int state, char *mode) {

//     auto status = ibv_modify_qp(qp, attr, state);

//     if (status == 0)
//         printf("QP changed to %s\n", mode);
//     else if (status == EINVAL)
//         printf("Invalid value provided in attr or in attr_mask for mode %s\n", mode);
//     else if (status == ENOMEM)
//         printf("Not enough resources to complete this operation for mode %s\n", mode);
//     else
//         printf("QP modify status: %i for mode %s\n",status, mode);

//     if (status != 0)
//         exit(EXIT_FAILURE);

// }





void RdmaHandler::cleanup() {

}

app_context* RdmaHandler::get_app_context() {
    return app_ctx;
}