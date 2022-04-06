#include <infiniband/verbs.h>
#include <malloc.h>
#include <unistd.h>

#define PORT_NUM 1
#define QKEY 0x11111111
#define GID_IDX 1
#define IB_PORT 1
#define MTU 1500
#define MSG_SIZE 1024
#define GRH_SIZE 40
#define CQ_SIZE 10
#define MAX_WR 10
#define MAX_SGE 1
#define MAX_QP 1024

int setup() {


    char devname[] = "hrn0_1";


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
        if (strcmp(ib_dev->name, devname) == 0) {
            found = true;
            break;
        }
        tmp_dev_list++;
        ib_dev = *tmp_dev_list;
    }

    if (!found){
        printf("could not find dev %s", devname);
        exit(EXIT_FAILURE);
    }
    
    printf("using dev name: %s\n", ib_dev->name);

    ibv_context* ctx = ibv_open_device(ib_dev);
    if (!ctx) {
        perror("error creating context");
        exit(EXIT_FAILURE);
    }

    ibv_free_device_list(dev_list);

    ibv_port_attr port_info = {};
    status = ibv_query_port(ctx, IB_PORT, &port_info);
    if (status == -1) {
        perror("could not get port info");
        exit(EXIT_FAILURE);
    }

    ibv_pd* pd = ibv_alloc_pd(ctx);
    if (!pd) {
	    fprintf(stderr, "Error, ibv_alloc_pd() failed\n");
	    exit(EXIT_FAILURE);
    }

    puts("PD created.");

    ibv_cq* cq = ibv_create_cq(ctx, CQ_SIZE, nullptr, nullptr, 0);
    if (!cq) {
        perror("could not create CQ");
        exit(EXIT_FAILURE);
    }

    puts("CQ created.");

    puts("creating QP");

    ibv_qp_init_attr_ex init_attr_ex = {0};
    init_attr_ex.send_cq = cq;
    init_attr_ex.recv_cq = cq;
    // init_attr_ex.srq = app_ctx->srq;
    
    init_attr_ex.cap     = {};
    init_attr_ex.cap.max_send_wr  = MAX_WR;
    init_attr_ex.cap.max_recv_wr  = MAX_WR;
    init_attr_ex.cap.max_send_sge = MAX_SGE;
    init_attr_ex.cap.max_recv_sge = MAX_SGE;
            
    init_attr_ex.qp_type = IBV_QPT_RC;
    init_attr_ex.sq_sig_all = 1;
    init_attr_ex.pd = pd;
    init_attr_ex.comp_mask  = IBV_QP_INIT_ATTR_PD;

    ibv_qp* qp = ibv_create_qp_ex(ctx, &init_attr_ex);
    if (!qp) {
        perror("could not create QP");
        exit(1);
    }

    puts("QP created.");

    ibv_qp_attr attr = {};
    attr.qp_state        = IBV_QPS_INIT;
    attr.pkey_index      = 0;
    attr.port_num        = PORT_NUM;
    attr.qp_access_flags = 0;

    int state = IBV_QP_STATE              |
                IBV_QP_PKEY_INDEX         |
                IBV_QP_PORT               |
                IBV_QP_ACCESS_FLAGS;  
    
    	if (ibv_modify_qp(qp, &attr,state)) {
		fprintf(stderr, "Failed to modify QP to INIT\n");
		exit(EXIT_FAILURE);
	}

    puts("QP switched to INIT");




    puts("setting up memory.");

    int mr_size =  ( MSG_SIZE + GRH_SIZE ) * MAX_WR;
    int alignment = sysconf(_SC_PAGESIZE);
    char* buf = (char*) memalign(alignment, mr_size);

    if (!buf) {
        perror("error creating memory buffer.");
        exit(EXIT_FAILURE);
    }

    ibv_mr* mr = ibv_reg_mr(pd, buf, mr_size, IBV_ACCESS_LOCAL_WRITE);
    if (!mr) {
        perror("error registering memory");
        exit(EXIT_FAILURE);
    }

    uint32_t msg_size = MSG_SIZE + GRH_SIZE;

    uint64_t mem_addr = reinterpret_cast<uint64_t>(buf);

    // SGE for send requests
    ibv_sge *sge = (ibv_sge*) calloc(1, sizeof sge);
    sge->addr = mem_addr;
    sge->length = 0;
    sge->lkey = mr->lkey;

    ibv_recv_wr rec_wr = {};
    rec_wr.wr_id = reinterpret_cast<uint64_t>(sge);
    rec_wr.sg_list = sge;
    rec_wr.num_sge = 1;

    ibv_recv_wr *bad_wr;

    if (ibv_post_recv(qp, &rec_wr, &bad_wr)) {
        perror("error posting RR in QP.");
        exit(EXIT_FAILURE);    
    }

    puts("memory posted to RQ");




    return 0;

};


int main(int argc, char* argv[]) {
    setup();
}