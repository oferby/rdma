#ifndef RDMA_HANDLER 

#define RDMA_HANDLER 

#include <stdlib.h>
#include <infiniband/verbs.h>
#include <endian.h>
#include <map>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <stdexcept>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <malloc.h>
#include <getopt.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MSG_FORMAT "%04x:%06x:%06x:%06x:%06x:%32s"
#define MSG_SIZE   1024
#define MSG_SSCAN  "%x:%x:%x:%x:%x:%s"
#define ADDR_FORMAT \
	"%8s: LID %04x, QPN RECV %06x SEND %06x, PSN %06x, SRQN %06x, GID %s\n"
#define TERMINATION_FORMAT "%s"
#define TERMINATION_MSG_SIZE 4
#define TERMINATION_MSG "END"
static int page_size;
static int use_odp;

#define PORT_NUM 1
#define QKEY 0x11111111
#define GID_IDX 1
#define IB_PORT 1
#define MTU 1500
#define GRH_SIZE 40
#define CQ_SIZE 10
#define MAX_WR 10
#define MAX_SGE 1
#define MAX_QP 1024

struct app_dest {
	ibv_gid gid;
	int lid;
	int recv_qpn;
	int send_qpn;
	int recv_psn;
	int send_psn;
	int srqn;
	int pp_cnt;
	int sockfd;
};

struct app_context {
	struct ibv_context	*ctx;
	struct ibv_comp_channel *channel;
	struct ibv_pd		*pd;
	struct ibv_mr		*mr;
	struct ibv_cq		*send_cq;
	struct ibv_cq		*recv_cq;
	struct ibv_srq		*srq;
	struct ibv_xrcd		*xrcd;
	struct ibv_qp		**recv_qp;
	struct ibv_qp		**send_qp;
	struct app_dest	*rem_dest;
	void			*buf;
	int			 lid;
	int			 sl;
	enum ibv_mtu		 mtu;
	int			 ib_port;
	int			 fd;
	int			 size;
	int			 num_clients;
	int			 num_tests;
	int			 use_event;
	int			 gidx;

	ibv_port_attr   *portinfo;
	char       		*devname;
};

struct neighbor {
    sockaddr_in *addr;
	app_dest *local_dest;
    app_dest *remote_dest;
    time_t lastHello;
};


class RdmaHandler {

private:

    app_context *app_ctx;
	app_dest *local_dest;
  
	std::map<uint32_t, ibv_qp*> qp_map;
    std::map <uint64_t,ibv_sge*> sge_map;
    std::vector<ibv_sge*> available_send_sge_vector;

    int status;
    ibv_wc wc;

	void setup_context();
	void setup_memory();
	void post_recv();
	void setup_xrc();
	// void create_local_dest();
	// void create_queue_pair();
	// void switch_to_init();
	// void do_qp_change(ibv_qp* qp, ibv_qp_attr *attr, int state, char *mode); 
    // void changeQueuePairState(app_context *app_ctx); 
    // void handle_rr();
    // void handle_sr();
    // void handle_wc();
	void cleanup();

public:
 	
	RdmaHandler(char*);
	app_context* get_app_context();
	app_dest* get_local_dest();
	void set_dest(app_dest* dest);
	void poll_complition();
	void send_to_dest(const char *data, size_t len);
	bool is_ready();
};

static void print_gid(const union ibv_gid *gid) {
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

static void print_dest(app_dest* dest) {
		static char gid[33];
        inet_ntop(AF_INET6, dest->gid.raw, gid, sizeof(gid));
	    printf(ADDR_FORMAT,
			dest->lid, dest->recv_qpn, dest->send_qpn, dest->recv_psn, dest->srqn, gid);
}


#endif

