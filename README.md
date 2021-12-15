test RDMA UD with UDP and epoll

 sudo yum install libibverbs-utils




sudo yum install ucx

ucx_info -d

sudo yum install libibverbs-utils

ibv_devinfo -vv


g++ app.cpp -o app -libverbs -std=c++11

# for Huawei NIC
- hiroce gids
