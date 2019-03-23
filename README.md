# swim-membership-protocol

## 1 编译运行

执行 `make` 命令以编译项目

执行 `./Application testcases/<test_name>.conf` 以运行驱动层程序

配置文件由4行配置信息构成：`MAX_NNB` 代表总节点的最大数量；`SINGLE_FAILURE` 代表是否启用多个同时的失效节点测试，0代表启用，1代表关闭；`MSG_DROP_PROB` 代表预设的信息丢失概率，这在真实的网络环境中很常见；`MSG_DROP` 代表是否启用信息丢失模式，0代表每条信息都不会丢失。

程序运行的调试 Log 会在运行后输出到 `dbg.log` 文件，在 `stdincludes.h` 内可以选择是否开启调试信息输出。

## 2 参考

Swim论文：http://www.cs.cornell.edu/Info/Projects/Spinglass/public_pdfs/SWIM.pdf

我的博客：https://hunterlxt.github.io/2018/12/01/SWIM-protocol/
