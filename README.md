test RDMA UD with UDP and epoll

 sudo yum install libibverbs-utils




sudo yum install ucx

ucx_info -d

sudo yum install libibverbs-utils

ibv_devinfo -vv


export LD_LIBRARY_PATH=/home/oferb/dev/rdma-core/build/lib:$LD_LIBRARY_PATH

g++ app.cpp -o app -libverbs -L /home/oferb/dev/rdma-core/build/lib

# for Huawei NIC
- hiroce gids
