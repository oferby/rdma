#include <stdlib.h>
#include <infiniband/verbs.h>
#include <endian.h>
#include <map>
#include <vector>

#ifndef RDMA_HANDLER 

#define RDMA_HANDLER 

#define PORT_NUM 1
#define QKEY 0x11111111
#define GID_IDX 0
#define IB_PORT 1
#define MSG_SIZE 1500
#define CQ_SIZE 10
#define MAX_WR 10
#define MAX_SGE 1



struct app_context {
	struct ibv_context	*ctx;
	struct ibv_comp_channel *channel;
	struct ibv_pd		*pd;
	struct ibv_mr		*mr;
	struct ibv_cq		*cq;
	struct ibv_qp		*qp;
	struct ibv_ah		*ah;
	char			*buf;
	int			 size;
	int			 send_flags;
	int			 rx_depth;
	int			 pending;
	struct ibv_port_attr     *portinfo;
   
    uint64_t wid = 0;
    std::map <uint64_t,ibv_sge*> *sge_map;
    std::vector<ibv_sge*> *available_send_sge_vector;

};

struct app_dest {
	uint16_t lid;
	int qpn;
	int psn;
	union ibv_gid *gid;
	ibv_ah *ah;
};


struct neighbor {
    sockaddr_in *addr;
    app_dest *dest;
    time_t lastHello;
};


class RdmaHandler {

private:

    app_context app_ctx;
    app_dest *local_dest;
    std::map <uint64_t,ibv_sge*> sge_map;
    std::vector<ibv_sge*> available_send_sge_vector;

    int status;
    ibv_wc wc;

	static void cleanup(app_context *ctx) {
        if(ctx->qp)
            ibv_destroy_qp(ctx->qp);

        if (ctx->cq)
            ibv_destroy_cq(ctx->cq);
        if (ctx->mr)
            ibv_dereg_mr(ctx->mr);

        if (ctx->pd)
            ibv_dealloc_pd(ctx->pd);

        if (ctx->ctx)
            ibv_close_device(ctx->ctx);

        if (ctx->buf)
            free(ctx->buf);

        free(ctx);

    }

	static void createQueuePair(app_context *app_ctx) {

		struct ibv_qp_attr attr;
		struct ibv_qp_init_attr init_attr = {
			.send_cq = app_ctx->cq,
			.recv_cq = app_ctx->cq,
			.cap     = {
				.max_send_wr  = MAX_WR,
				.max_recv_wr  = MAX_WR,
				.max_send_sge = MAX_SGE,
				.max_recv_sge = MAX_SGE,
                
			},
			.qp_type = IBV_QPT_UD,
            .sq_sig_all = 1
		};

		app_ctx->qp = ibv_create_qp(app_ctx->pd, &init_attr);
		if (!app_ctx->qp)  {
			fprintf(stderr, "Couldn't create QP\n");
			cleanup(app_ctx);
            exit(EXIT_FAILURE);
		}

    }

	static void do_qp_change(ibv_qp* qp, ibv_qp_attr *attr, int state, char *mode) {

        auto status = ibv_modify_qp(qp, attr, state);

        if (status == 0)
            printf("QP changed to %s\n", mode);
        else if (status == EINVAL)
            printf("Invalid value provided in attr or in attr_mask for mode %s\n", mode);
        else if (status == ENOMEM)
            printf("Not enough resources to complete this operation for mode %s\n", mode);
        else
            printf("QP modify status: %i for mode %s\n",status, mode);

        if (status != 0)
            exit(EXIT_FAILURE);

    }

	static void setup_memory(app_context *app_ctx) {
        
        puts("setting up memory.");

        int mr_size =  ( MSG_SIZE + 40 ) * MAX_WR;
        int alignment = sysconf(_SC_PAGESIZE);
        app_ctx->buf = (char*) memalign(alignment, mr_size);

        if (!app_ctx->buf) {
            perror("error creating memory buffer.");
            cleanup(app_ctx);
            exit(EXIT_FAILURE);
        }

        app_ctx->mr = ibv_reg_mr(app_ctx->pd, app_ctx->buf, mr_size, IBV_ACCESS_LOCAL_WRITE);
        if (!app_ctx->mr) {
            perror("error registering memory");
            cleanup(app_ctx);
            exit(EXIT_FAILURE);
        }

        uint32_t msg_size = MSG_SIZE + 40;

    
        uint64_t mem_addr = (uintptr_t) app_ctx->buf;

        ibv_sge *sge = (ibv_sge*) calloc(1, sizeof sge);
        sge->addr = mem_addr;
        sge->length = 0;
        sge->lkey = app_ctx->mr->lkey;

        app_ctx->available_send_sge_vector->push_back(sge);


        mem_addr += msg_size;

        sge = (ibv_sge*) calloc(1, sizeof sge);
        sge->addr = mem_addr;
        sge->length = msg_size;
        sge->lkey = app_ctx->mr->lkey;

        ibv_recv_wr rec_wr = {
            .wr_id = app_ctx->wid++,
            .sg_list = sge,
            .num_sge = 1, 
        };

        ibv_recv_wr *bad_wr;

        if (ibv_post_recv(app_ctx->qp, &rec_wr, &bad_wr)) {
            perror("error posting RR.");
            cleanup(app_ctx);
            exit(EXIT_FAILURE);    
        } 

        
        puts("memory and WRs added.");

    }

	static void changeQueuePairState(app_context *app_ctx) {

		ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_INIT,
            .qkey            = QKEY,
			.pkey_index      = 0,
			.port_num        = PORT_NUM
		};

        int state = IBV_QP_STATE              |
				    IBV_QP_PKEY_INDEX         |
				    IBV_QP_PORT               |
				    IBV_QP_QKEY;

        do_qp_change(app_ctx->qp, &attr, state, (char*) "INIT");

        memset(&attr, 0, sizeof(attr));
        attr.qp_state = ibv_qp_state::IBV_QPS_RTR;

        do_qp_change(app_ctx->qp, &attr, IBV_QP_STATE, (char*) "RTR");

        memset(&attr, 0, sizeof(attr));
        attr.qp_state = ibv_qp_state::IBV_QPS_RTS;
        attr.sq_psn = 0;

        do_qp_change(app_ctx->qp, &attr, IBV_QP_STATE | IBV_QP_SQ_PSN, (char*) "RTS");

        puts("QP ready.");

    }
	
	static void setup_context(app_context *app_ctx) {

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
        while (1)
        {
            if (strcmp(ib_dev->name,"rxe0") == 0)
                break;
            tmp_dev_list++;
            ib_dev = *tmp_dev_list;
        }
        
        printf("using dev name: %s\n", ib_dev->name);

        app_ctx->ctx = ibv_open_device(ib_dev);
        if (!app_ctx->ctx) {
            perror("error creating context");
            exit(EXIT_FAILURE);
        }

        ibv_free_device_list(dev_list);

        ibv_port_attr port_attr;
        status = ibv_query_port(app_ctx->ctx, IB_PORT, &port_attr);
        if (status == -1) {
            perror("could not get port info");
            exit(EXIT_FAILURE);
        }
        app_ctx->portinfo = &port_attr;

        app_ctx->pd = ibv_alloc_pd(app_ctx->ctx);

        app_ctx->cq = ibv_create_cq(app_ctx->ctx, CQ_SIZE, nullptr, nullptr, 0);

        createQueuePair(app_ctx);

        changeQueuePairState(app_ctx);

        setup_memory(app_ctx);       

    }
	
    void handle_rr() {
        
        printf("WC: received %i\n",wc.byte_len);

        if (wc.wc_flags && IBV_WC_GRH) {
            puts("GRH exists in payload.");

        }




    }

    void handle_sr(){

        printf("WC: sent %i bytes\n",wc.byte_len);

    }

    void handle_wc() {

        puts("handling WC.");

        switch (wc.opcode) {

            case IBV_WC_SEND :
                handle_sr();
                break;

            case IBV_WC_RECV :
                handle_rr();
                break;

            default : 
                puts("got wrong WC opcode");
                break;

        }


    }


public:
 	
	RdmaHandler();
	app_context* get_app_context();
	app_dest* get_local_dest();
	void poll_complition();
	void create_send_request(const char *data, size_t len, app_dest *dest);
};


void print_gid(const union ibv_gid *gid) {
		static char gid_tmp[33];
        inet_ntop(AF_INET6, gid, gid_tmp, INET6_ADDRSTRLEN);
	    printf(" ** GID %s\n", gid_tmp);

}



static void wire_gid_to_gid(const char *wgid, union ibv_gid *gid) { 

	char tmp[9];
	__be32 v32;
	int i;
	uint32_t tmp_gid[4];

	for (tmp[8] = 0, i = 0; i < 4; ++i) {
		memcpy(tmp, wgid + i * 8, 8);
		sscanf(tmp, "%x", &v32);
		tmp_gid[i] = be32toh(v32);
	}
	memcpy(gid, tmp_gid, sizeof(*gid));

}

static void gid_to_wire_gid(const union ibv_gid *gid, char wgid[]) {

	uint32_t tmp_gid[4];
	int i;

	memcpy(tmp_gid, gid, sizeof(tmp_gid));
	for (i = 0; i < 4; ++i)
		sprintf(&wgid[i * 8], "%08x", htobe32(tmp_gid[i]));

}


void print_dest(struct app_dest * dest) {
		static char gid[33];
        inet_ntop(AF_INET6, dest->gid, gid, sizeof gid);
	    printf("  address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x: GID %s\n",
			dest->lid, dest->qpn, dest->psn, gid);
}


#endif

