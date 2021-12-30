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


#define PORT_NUM 1
#define QKEY 0x11111111
#define GID_IDX 1
#define IB_PORT 1
#define MSG_SIZE 1500
#define GRH_SIZE 40
#define CQ_SIZE 10
#define MAX_WR 10
#define MAX_SGE 1
#define MAX_QP 1024

struct app_context {
	ibv_context	*ctx;
	ibv_pd		*pd;
	ibv_mr		*mr;
	ibv_cq		*cq;
	ibv_srq		*srq;
	ibv_qp		*qp;
	ibv_ah		*ah;
	char			*buf;
	int			 size;
	int			 send_flags;
	int			 pending;
	ibv_port_attr     *portinfo;
    char         *devname;
   
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
	app_dest *local_dest;
    app_dest *dest;
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

	void create_srq();
	
	void create_queue_pair();

	void setup_memory();

	void create_local_dest();

	

	void do_qp_change(ibv_qp* qp, ibv_qp_attr *attr, int state, char *mode); 

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
	// void poll_complition();
	// void create_send_request(const char *data, size_t len, app_dest *dest);
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

static void print_dest(app_dest * dest) {
		static char gid[33];
        inet_ntop(AF_INET6, dest->gid, gid, sizeof gid);
	    printf("  address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x: GID %s\n",
			dest->lid, dest->qpn, dest->psn, gid);
}


#endif

