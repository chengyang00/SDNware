# 预测代码
修改rdma-core中的libibverbs.so的源码`/rdma-core/providers/mlx5/qp.c`，修改的函数为`_mlx5_post_send`，实现work request的读取，从而预测RDMA的通信流量。

## 代码功能
1. 读取发送数据大小，并添加包头，得到实际的预测流量
2. 将目的IP和预测流量写入创建的共享内存表，以提供sync代码读取

## 编译
在`rdma-core`目录下运行build代码，生成libibverbs.so：
```
bash build.sh
```
Linux系统中的`/usr/lib/x86_64-linux-gnu/libibverbs.so.1`是一个软链接，默认连接到动态链接库`/usr/lib/x86_64-linux-gnu/libibverbs.so.1.14.37.0`（这里的libibverbs是1.14.37版本，不同系统的版本可能不一样）。我们需要将这个软链接连接到我们修改后的动态链接库：（我修改的rdma-core版本为1.14.37.8）
```
sudo ln -sf /home/sdn/ybd/rdma-core-37.8/build/lib/libibverbs.so.1.14.37.8 /usr/lib/x86_64-linux-gnu/libibverbs.so.1
```
恢复这个软链接到默认库的指令：
```
sudo ln -sf /usr/lib/x86_64-linux-gnu/libibverbs.so.1.14.37.0 /usr/lib/x86_64-linux-gnu/libibverbs.so.1
```