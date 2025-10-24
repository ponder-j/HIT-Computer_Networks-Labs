# Lab2-2 文件传输 (SR 协议)

本实现基于 UDP + Selective Repeat (选择重传) 实现可靠的文件上传：
- 客户端: 读取本地文件 -> 拆分分片 -> SR 滑动窗口发送 -> 等待选择性 ACK -> 超时重传。
- 服务器: 接收文件信息帧 -> 对每个数据分片发送 ACK -> 乱序去重写入 -> 收到所有分片后落盘。

与 `Lab2-2/SR_protocol` 里的文本 SR 测试相互独立；这里用于实际文件传输。

## 协议帧格式

统一 `DataFrame` 结构（客户端和服务器一致）：
- seq: 无符号字节，序列号空间大小 `SEQ_SIZE`，必须满足 `SEQ_SIZE >= 2 * WINDOW_SIZE`。
- fileSize: 仅信息帧(flag=4)使用。
- offset: 数据在文件中的字节偏移。
- dataLen: 数据长度，<= DATA_SIZE。
- data: 数据内容，大小 `DATA_SIZE`。
- flag: 0=数据帧，1=最后一帧，2=ACK，4=信息帧。

ACK 使用同一结构，`flag=2`，仅 `seq` 有效。

## 目录
- `server.cpp` 服务端，接收并保存到 `received_files/`。
- `client.cpp` 客户端，发送本地文件。

## 构建
VS Code 已配置“编译 C++ (g++)”任务用于当前打开文件：
1. 打开 `server.cpp`，执行任务构建为 `server.exe`。
2. 打开 `client.cpp`，执行任务构建为 `client.exe`。

若使用命令行（Windows PowerShell 示例）：
```
# 在项目根工作区下
# 构建服务端
g++ -g .\Lab2-2\file_transfer\server.cpp -o .\Lab2-2\file_transfer\server.exe -lws2_32 -fexec-charset=GBK -finput-charset=UTF-8
# 构建客户端
g++ -g .\Lab2-2\file_transfer\client.cpp -o .\Lab2-2\file_transfer\client.exe -lws2_32 -fexec-charset=GBK -finput-charset=UTF-8
```

## 运行
1. 启动服务端：运行 `server.exe`（首次自动创建 `received_files/`）。
2. 启动客户端：运行 `client.exe`，输入：
   - `-upload <文件路径>` 上传指定文件。
   - `-quit` 退出客户端。

服务端会在 `received_files/` 下生成同名文件。

## 说明与参数
- 窗口与序列空间：`WINDOW_SIZE=10`，`SEQ_SIZE=32`，满足 SR 唯一确认性约束。
- 发送端超时：`TIMEOUT_MS=500ms`，每个分片独立计时；达到 `MAX_RETRY` 将终止。
- 服务端 ACK 丢包模拟：`RECV_ACK_LOSS=0.2`，可在 `server.cpp` 中调整。

## 注意
- 两端结构体字段顺序必须完全一致。
- 使用 `offset` + `dataLen` 支持乱序落盘与去重，避免重复写入。
- 终止条件：服务端收到全部分片（按 `fileSize` 推导分片数）。

## 扩展思路
- 增加限速/拥塞控制。
- 改为单独的 `AckFrame`，减小 ACK 包大小。
- 文件下载/双向传输。
