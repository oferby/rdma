test RDMA UD with UDP and epoll

 sudo yum install libibverbs-utils




sudo yum install ucx

ucx_info -d

sudo yum install libibverbs-utils

ibv_devinfo -vv


g++ app.cpp -o app -libverbs -std=c++11

./app  hrn0_1 192.168.60.5


# for Huawei NIC
- hiroce gids
