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
#include <fcntl.h>



#define PORT_NUM 1
#define QKEY 0x11111111
#define GID_IDX 1
#define IB_PORT 1
#define MTU 1500
#define IB_MSG_SIZE 1024
#define GRH_SIZE 40
#define CQ_SIZE 10
#define MAX_WR 10
#define MAX_SGE 1
#define MAX_QP 1024

#define ADDR_FORMAT \
	"%8s: LID %04x, QPN RECV %06x SEND %06x, PSN %06x, SRQN %06x, GID %s\n"
#define MSG_SIZE   66
#define MSG_FORMAT "%04x:%06x:%06x:%06x:%06x:%32s"
#define MSG_SSCAN  "%x:%x:%x:%x:%x:%s"
#define TERMINATION_FORMAT "%s"
#define TERMINATION_MSG_SIZE 4
#define TERMINATION_MSG "END"

static int page_size;
static int use_odp;


struct ib_conn_info {
	int recv_qpn;
	int send_qpn;
	int recv_psn;
	int send_psn;
	int srqn;
};

struct ib_connection {

	ibv_qp*				recv_qp;
	ibv_qp*				send_qp;
	ib_conn_info	conn_info;
		
};

struct ib_worker {
	ibv_pd*			pd;
	ibv_mr*			mr;
	ibv_comp_channel*	channel;
	int					fd;
};

struct app_context {
	ibv_context*	ctx;
	ibv_xrcd*		xrcd;
	ibv_srq*		srq;
	uint32_t 		srq_num;
	ibv_cq* 			send_cq;
	ibv_cq* 			recv_cq;	
	int			 	lid;
	ibv_gid 		gid;
	char*			gidc;
	int			 	sl;
	enum ibv_mtu	mtu;
	int			 	ib_port;

	ibv_port_attr*	portinfo;
	char*			devname;

	ib_worker* worker;

};


class RdmaHandler {

private:
	app_context* app_ctx;

	std::map<const in_addr_t*, ib_connection*> qp_map;

    std::map <uint64_t,ibv_sge*> sge_map;
    std::vector<ibv_sge*> available_send_sge_vector;

    int status;
    ibv_wc wc;

	void setup_context();
	void setup_memory();
	void setup_xrc(ib_connection*);
	void create_qps(ib_connection*);
	char* get_local_conn_info(ib_connection* conn);
	void post_recv();
	
	
    // void handle_rr();
    // void handle_sr();
    // void handle_wc();
	void cleanup();
	void print_dest(ib_conn_info*);

public:
 	
	RdmaHandler(char* device);
	char* get_hello_msg(const in_addr_t* clientaddr);
	int create_ib_connection(const sockaddr_in* client, char* msg);
	void send(const sockaddr_in* client, const char* data, size_t len);
};






static char* get_gid(const union ibv_gid *gid) {
		static char gid_tmp[33];
        inet_ntop(AF_INET6, gid, gid_tmp, INET6_ADDRSTRLEN);
	    printf(" ** GID %s\n", gid_tmp);
        return gid_tmp;

};

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

};

static void gid_to_wire_gid(const union ibv_gid *gid, char wgid[]) {

	uint32_t tmp_gid[4];
	int i;

	memcpy(tmp_gid, gid, sizeof(tmp_gid));
	for (i = 0; i < 4; ++i)
		sprintf(&wgid[i * 8], "%08x", htobe32(tmp_gid[i]));

};
#endif

