#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <ctime>
#include <cstdlib>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 服务器配置
#define SERVER_PORT 12340
#define BUFFER_SIZE 1024
#define TIMEOUT_MS 2000

// 数据帧结构
struct DataFrame {
    unsigned char seq;      // 序列号 (0或1)
    char data[BUFFER_SIZE]; // 数据
    unsigned char flag;     // 标志位: 0=数据帧, 1=结束帧, 2=ACK帧
};

// 全局变量
unsigned char sendSeq = 0;  // 发送序列号
unsigned char recvSeq = 0;  // 接收序列号
float lossRate = 0.2f;      // ACK丢包率（可修改此值来改变丢包概率，范围0.0-1.0）

// 模拟丢包
bool simulateLoss() {
    if (lossRate <= 0.0f) return false;
    return ((float)rand() / RAND_MAX) < lossRate;
}

// 获取当前时间字符串
string getCurrentTime() {
    time_t now = time(0);
    char* dt = ctime(&now);
    string timeStr(dt);
    timeStr.pop_back();
    return timeStr;
}

// 发送数据帧并等待ACK（使用专用接收循环）
bool sendDataFrame(SOCKET sock, sockaddr_in& clientAddr, const char* data, bool isEnd = false) {
    DataFrame frame;
    frame.seq = sendSeq;
    strcpy_s(frame.data, data);
    frame.flag = isEnd ? 1 : 0;

    char buffer[sizeof(DataFrame)];
    memcpy(buffer, &frame, sizeof(DataFrame));

    int attempts = 0;
    const int MAX_ATTEMPTS = 5;

    while (attempts < MAX_ATTEMPTS) {
        // 发送数据帧
        sendto(sock, buffer, sizeof(DataFrame), 0,
               (sockaddr*)&clientAddr, sizeof(clientAddr));

        cout << "[发送->客户端] Seq=" << (int)sendSeq
             << ", 数据=\"" << data << "\", 尝试=" << (attempts + 1) << endl;

        // 设置接收超时
        DWORD timeout = TIMEOUT_MS;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

        // 等待ACK
        while (true) {
            DataFrame ackFrame;
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvLen = recvfrom(sock, (char*)&ackFrame, sizeof(DataFrame), 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                if (ackFrame.flag == 2) { // ACK帧
                    cout << "[接收ACK] ACK=" << (int)ackFrame.seq << endl;

                    if (ackFrame.seq == sendSeq) {
                        cout << "[成功] 收到正确ACK" << endl;
                        sendSeq = 1 - sendSeq; // 切换序列号

                        // 恢复非阻塞
                        timeout = 0;
                        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

                        return true;
                    } else {
                        cout << "[错误] ACK序列号不匹配，期望=" << (int)sendSeq
                             << ", 收到=" << (int)ackFrame.seq << endl;
                    }
                } else {
                    // 收到非ACK帧，可能是客户端发来的数据，忽略或缓存
                    cout << "[警告] 等待ACK时收到数据帧 Seq=" << (int)ackFrame.seq << endl;
                }
            } else if (recvLen == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAETIMEDOUT) {
                    cout << "[超时] 准备重传..." << endl;
                    break; // 退出接收循环，重传
                }
            }
        }

        attempts++;
    }

    cout << "[失败] 达到最大重传次数" << endl;

    // 恢复非阻塞
    DWORD timeout = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    return false;
}

// 发送ACK，返回是否成功发送
bool sendAck(SOCKET sock, sockaddr_in& clientAddr, unsigned char ackSeq) {
    // 模拟ACK丢失
    if (simulateLoss()) {
        cout << "[模拟] ACK丢失!" << endl;
        return false;  // ACK丢失，返回false
    }

    DataFrame ackFrame;
    ackFrame.seq = ackSeq;
    ackFrame.flag = 2; // ACK帧
    strcpy_s(ackFrame.data, "ACK");

    sendto(sock, (char*)&ackFrame, sizeof(DataFrame), 0,
           (sockaddr*)&clientAddr, sizeof(clientAddr));

    cout << "[发送ACK->客户端] ACK=" << (int)ackSeq << endl;
    return true;  // ACK成功发送，返回true
}

int main() {
    // 初始化随机数生成器
    srand((unsigned int)time(NULL));

    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup失败" << endl;
        return 1;
    }

    // 创建UDP socket
    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        cout << "创建socket失败" << endl;
        WSACleanup();
        return 1;
    }

    // 绑定地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "绑定失败" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "========================================" << endl;
    cout << "双向停等协议服务器已启动" << endl;
    cout << "监听端口: " << SERVER_PORT << endl;
    cout << "支持双向数据传输" << endl;
    cout << "ACK丢包率: " << (lossRate * 100) << "%" << endl;
    cout << "========================================" << endl;

    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    bool clientConnected = false;

    // 设置非阻塞接收（超时立即返回）
    DWORD timeout = 100; // 100ms轮询
    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    // 主循环
    while (true) {
        char recvBuffer[2048];  // 足够大的缓冲区，可以接收DataFrame或命令

        // 接收消息
        int recvLen = recvfrom(serverSocket, recvBuffer, 2048, 0,
                              (sockaddr*)&clientAddr, &clientAddrLen);

        if (recvLen > 0) {
            // 判断是命令还是数据帧
            if (recvLen == sizeof(DataFrame)) {
                DataFrame* frame = (DataFrame*)recvBuffer;

                if (frame->flag == 2) {
                    // 收到ACK，这是回给sendDataFrame的，但可能已被处理
                    // 这里忽略，避免干扰
                    continue;
                }

                // 收到数据帧
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);

                cout << "\n[接收<-客户端] Seq=" << (int)frame->seq
                     << ", 数据=\"" << frame->data << "\"" << endl;

                // 检查序列号
                if (frame->seq == recvSeq) {
                    cout << "[接受] 序列号正确" << endl;

                    // 发送ACK，只有成功发送才处理数据和切换序列号
                    if (sendAck(serverSocket, clientAddr, recvSeq)) {
                        // ACK成功发送，处理数据
                        cout << "[数据] " << frame->data << endl;

                        // 切换期望序列号
                        recvSeq = 1 - recvSeq;

                        // 检查是否结束
                        if (frame->flag == 1) {
                            cout << "[完成] 收到结束帧" << endl;
                        }
                    }
                    // 如果ACK丢失，不处理数据，不切换序列号，等待客户端重传
                } else {
                    cout << "[错误] 序列号不匹配，期望=" << (int)recvSeq
                         << ", 实际=" << (int)frame->seq << endl;
                    // 重发上一个ACK
                    sendAck(serverSocket, clientAddr, 1 - recvSeq);
                }

                clientConnected = true;
            } else {
                // 收到命令
                recvBuffer[recvLen] = '\0';
                string command(recvBuffer);

                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);

                cout << "\n[命令] 来自 " << clientIP << " - " << command << endl;

                if (command == "-time") {
                    string timeStr = getCurrentTime();
                    sendto(serverSocket, timeStr.c_str(), timeStr.length(), 0,
                          (sockaddr*)&clientAddr, clientAddrLen);
                    cout << "[响应] 时间: " << timeStr << endl;
                }
                else if (command == "-quit") {
                    string goodbye = "Good bye!";
                    sendto(serverSocket, goodbye.c_str(), goodbye.length(), 0,
                          (sockaddr*)&clientAddr, clientAddrLen);
                    cout << "[响应] 客户端退出" << endl;
                    clientConnected = false;
                    sendSeq = 0;
                    recvSeq = 0;
                }
                else if (command == "-recv") {
                    // 服务器准备接收客户端发送的数据
                    cout << "\n[双向传输测试] 准备接收客户端数据" << endl;
                    cout << "======================================" << endl;

                    // 重置接收序列号
                    recvSeq = 0;

                    // 发送确认响应
                    string ack = "READY";
                    sendto(serverSocket, ack.c_str(), ack.length(), 0,
                          (sockaddr*)&clientAddr, clientAddrLen);

                    cout << "[就绪] 等待客户端发送数据帧..." << endl;
                }
                else if (command == "-send") {
                    // 服务器向客户端发送数据
                    cout << "\n[双向传输测试] 服务器->客户端" << endl;
                    cout << "======================================" << endl;

                    const char* testData[] = {
                        "服务器消息1: 双向通信测试",
                        "服务器消息2: 停等协议",
                        "服务器消息3: 可靠传输",
                        "服务器消息4: 完成测试"
                    };

                    sendSeq = 0; // 重置发送序列号

                    for (int i = 0; i < 4; i++) {
                        bool isLast = (i == 3);
                        if (!sendDataFrame(serverSocket, clientAddr, testData[i], isLast)) {
                            cout << "[失败] 数据发送失败" << endl;
                            break;
                        }
                    }

                    cout << "======================================" << endl;
                    cout << "[完成] 服务器数据发送完毕" << endl;
                }
                else {
                    // 回显
                    sendto(serverSocket, recvBuffer, recvLen, 0,
                          (sockaddr*)&clientAddr, clientAddrLen);
                }

                clientConnected = true;
            }
        } else if (recvLen == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                // 超时，继续轮询
                continue;
            }
        }
    }

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
