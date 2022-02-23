#include "rdma_handler.h"


RdmaHandler::RdmaHandler(char *devname) {
        
        app_ctx = new app_context();
        app_ctx->worker = new ib_worker();

        app_ctx->devname = devname;
        
        setup_context();

        
}


void RdmaHandler::setup_context() {

    ibv_device   **dev_list, **tmp_dev_list;
    ibv_device   *ib_dev;
    ibv_xrcd_init_attr xrcd_attr {0};
    ibv_srq_init_attr_ex attr {0};

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

    app_ctx->worker->pd = ibv_alloc_pd(app_ctx->ctx);
    if (!app_ctx->worker->pd) {
	    perror("Error, ibv_alloc_pd() failed");
	    exit(EXIT_FAILURE);
    }
    
    puts("Creating XRC Domain");

    app_ctx->worker->fd = open("/tmp/xrc_domain", O_RDONLY | O_CREAT, S_IRUSR | S_IRGRP);
	if (app_ctx->worker->fd < 0) {
		fprintf(stderr,
			"Couldn't create the file for the XRC Domain "
			"but not stopping %d\n", errno);
		app_ctx->worker->fd = -1;
	}

    xrcd_attr.comp_mask = IBV_XRCD_INIT_ATTR_FD | IBV_XRCD_INIT_ATTR_OFLAGS;
	xrcd_attr.fd = app_ctx->worker->fd;
	xrcd_attr.oflags = O_CREAT;
	app_ctx->xrcd = ibv_open_xrcd(app_ctx->ctx, &xrcd_attr);
	if (!app_ctx->xrcd) {
		fprintf(stderr, "Couldn't Open the XRC Domain %d\n", errno);
        exit(EXIT_FAILURE);
	}

    if (ibv_query_gid(app_ctx->ctx, IB_PORT, GID_IDX,
            &app_ctx->gid)) {
        fprintf(stderr, "can't read sgid of index %d\n",
            GID_IDX);
        exit(EXIT_FAILURE);
    }

    app_ctx->gidc = get_gid(&app_ctx->gid);

    puts("Create CQ");
    // ctx.num_clients 
	app_ctx->recv_cq = ibv_create_cq(app_ctx->ctx, 1, &app_ctx->recv_cq,
				    app_ctx->worker->channel, 0);
	if (!app_ctx->recv_cq) {
		fprintf(stderr, "Couldn't create recv CQ\n");
		exit(EXIT_FAILURE);
	}

    // ctx.num_clients
	app_ctx->send_cq = ibv_create_cq(app_ctx->ctx, 1, NULL, NULL, 0);
	if (!app_ctx->send_cq) {
		fprintf(stderr, "Couldn't create send CQ\n");
		exit(EXIT_FAILURE);
	}

    puts("Create SRQ.");
    // ctx.num_clients
	attr.attr.max_wr = 1;
	attr.attr.max_sge = 1;
	attr.comp_mask = IBV_SRQ_INIT_ATTR_TYPE | IBV_SRQ_INIT_ATTR_XRCD |
			 IBV_SRQ_INIT_ATTR_CQ | IBV_SRQ_INIT_ATTR_PD;
	attr.srq_type = IBV_SRQT_XRC;
	attr.xrcd = app_ctx->xrcd;
	attr.cq = app_ctx->recv_cq;
	attr.pd = app_ctx->worker->pd;

	app_ctx->srq = ibv_create_srq_ex(app_ctx->ctx, &attr);
	if (!app_ctx->srq)  {
		fprintf(stderr, "Couldn't create SRQ\n");
		exit(EXIT_FAILURE);
	}

    if (ibv_get_srq_num(app_ctx->srq, &app_ctx->srq_num)) {
		fprintf(stderr, "Couldn't get SRQ num\n");
		exit(EXIT_FAILURE);
	}


    app_ctx->worker->channel = ibv_create_comp_channel(app_ctx->ctx);
    if (!app_ctx->worker->channel) {
        perror("Couldn't create completion channel");
        exit(EXIT_FAILURE);
    }

    setup_memory();

    puts("context setup done.");

}

void RdmaHandler::setup_memory() {
    
    puts("setting up memory.");

    int mr_size =  ( IB_MSG_SIZE + GRH_SIZE ) * MAX_WR;
    int alignment = sysconf(_SC_PAGESIZE);
    char* buf = (char*) memalign(alignment, mr_size);

    if (!buf) {
        perror("error creating memory buffer.");
        cleanup();
        exit(EXIT_FAILURE);
    }

    app_ctx->worker->mr = ibv_reg_mr(app_ctx->worker->pd, buf, mr_size, IBV_ACCESS_LOCAL_WRITE);
    if (!app_ctx->worker->mr) {
        perror("error registering memory");
        cleanup();
        exit(EXIT_FAILURE);
    }

    uint32_t msg_size = IB_MSG_SIZE + GRH_SIZE;

    uint64_t mem_addr = reinterpret_cast<uint64_t>(buf);

    // SGE for send requests
    ibv_sge *sge = (ibv_sge*) calloc(1, sizeof sge);
    sge->addr = mem_addr;
    sge->length = 0;
    sge->lkey = app_ctx->worker->mr->lkey;

    this->available_send_sge_vector.push_back(sge);

    // SGE for receive requests
    for (int i = 0; i < 2; i++) {

        mem_addr += msg_size;

        sge = (ibv_sge*) calloc(1, sizeof sge);
        sge->addr = mem_addr;
        sge->length = msg_size;
        sge->lkey = app_ctx->worker->mr->lkey;

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

char* RdmaHandler::get_hello_msg(const in_addr_t* clientaddr, ib_connection* conn) {
    
    // this side initiate the call   
    
    if (conn == nullptr)
        conn = get_ib_connection(clientaddr);

    return get_local_conn_info(conn->local);

}

char* RdmaHandler::get_hello_msg_response(const in_addr_t* clientaddr, char* buf) {

    // other side initiate the call  
    
    ib_connection* conn = get_ib_connection(clientaddr);

    set_remote_dest(conn, buf);
    
    return get_hello_msg(clientaddr, conn);
}


ib_connection* RdmaHandler::get_ib_connection(const in_addr_t* clientaddr) {

    ib_connection* conn = new ib_connection();
    conn->local = new ib_dest();
    conn->local->info = new ib_info();
    conn->local->info->srqn = app_ctx->srq_num;
    
    inet_ntop(AF_INET6, app_ctx->gid.raw, conn->local->info->gidc, sizeof(conn->local->info->gidc));

    conn_map[clientaddr] = conn;

    return conn;

}


void RdmaHandler::set_remote_dest(ib_connection* conn, char* buf) {

    puts("setting remote IB info");

    ib_dest* remote = new ib_dest();
    remote->info = new ib_info;
    conn->remote = remote;

    sscanf(buf, MSG_SSCAN, &remote->info->lid, &remote->info->recv_qpn,
		&remote->info->send_qpn, &remote->info->send_psn, &remote->info->srqn, remote->info->gidc);
    const char* s = "remote";
    print_dest(s, remote->info);

};


char* RdmaHandler::get_local_conn_info(ib_dest* dest) {

    create_qps(dest);

    char* msg = new char[MSG_SIZE];
    const char* s = "local";
    print_dest(s, dest->info);

    // gid_to_wire_gid(&dest->info->gid, dest->info->gidc);
	
    sprintf(msg, MSG_FORMAT, app_ctx->lid, dest->info->recv_qpn,
		dest->info->send_qpn, dest->info->recv_psn,
		dest->info->srqn, dest->info->gidc);

    return msg;
}


void RdmaHandler::create_qps(ib_dest*  dest ) {

    ibv_qp_init_attr_ex init {0};
	ibv_qp_attr mod;

    puts("setting up receive QPs");

    init.qp_type = IBV_QPT_XRC_RECV;
    init.comp_mask = IBV_QP_INIT_ATTR_XRCD;
    init.xrcd = app_ctx->xrcd;

    
     dest->recv_qp = ibv_create_qp_ex(app_ctx->ctx, &init);
    if (!dest->recv_qp)  {
        perror("Couldn't create recv QP");
        exit(EXIT_FAILURE);
    }

    dest->info->recv_qpn =  dest->recv_qp->qp_num;

    mod.qp_state        = IBV_QPS_INIT;
    mod.pkey_index      = 0;
    mod.port_num        = IB_PORT;
    mod.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;

    if (ibv_modify_qp( dest ->recv_qp, &mod,
                IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                IBV_QP_PORT | IBV_QP_ACCESS_FLAGS)) {
        perror("Failed to modify recv QP to INIT");
        exit(EXIT_FAILURE);
    }

    puts("setting up send QPs");

    memset(&init, 0, sizeof init);
    init.qp_type	      = IBV_QPT_XRC_SEND;
    init.send_cq	      = app_ctx->send_cq;
    init.cap.max_send_wr  = 1;
    init.cap.max_send_sge = 1;
    init.comp_mask	      = IBV_QP_INIT_ATTR_PD;
    init.pd		          = app_ctx->worker->pd;

     dest ->send_qp = ibv_create_qp_ex(app_ctx->ctx, &init);
    if (! dest ->send_qp)  {
        perror("Couldn't create send QP");
        exit(EXIT_FAILURE);
    }
    
     dest->info->send_qpn =  dest ->send_qp->qp_num;

    mod.qp_state        = IBV_QPS_INIT;
    mod.pkey_index      = 0;
    mod.port_num        = IB_PORT;
    mod.qp_access_flags = 0;

    if (ibv_modify_qp( dest ->send_qp, &mod,
                IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                IBV_QP_PORT | IBV_QP_ACCESS_FLAGS)) {
        perror("Failed to modify send QP to INIT");
        exit(EXIT_FAILURE);
    }

    puts("QPs created.");

}





void RdmaHandler::cleanup() {


};

void RdmaHandler::print_dest(const char* side, ib_info* info) {
		
        
        printf(ADDR_FORMAT, side, app_ctx->lid, info->recv_qpn,
                info->send_qpn, info->recv_psn,
                info->srqn, info->gidc);

}