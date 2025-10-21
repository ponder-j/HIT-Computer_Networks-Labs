#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 服务器配置
#define SERVER_PORT 12340
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

// 数据帧结构
struct DataFrame {
    unsigned char seq;      // 序列号 (0或1)
    char data[BUFFER_SIZE]; // 数据
    unsigned char flag;     // 标志位: 0=数据帧, 1=结束帧
};

// ACK帧结构
struct AckFrame {
    unsigned char ack;      // ACK序列号
};

// 全局变量
float lossRate = 0.0f; // 丢包率

// 模拟丢包
bool simulateLoss() {
    if (lossRate <= 0.0f) return false;
    float random = (float)rand() / RAND_MAX;
    return random < lossRate;
}

int main() {
    srand((unsigned int)time(NULL));

    // 初始化Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cout << "WSAStartup失败: " << result << endl;
        return 1;
    }

    // 创建UDP socket
    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        cout << "创建socket失败: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // 设置服务器地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    cout << "======================================" << endl;
    cout << "停等协议客户端" << endl;
    cout << "服务器地址: " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "======================================" << endl;
    cout << "\n可用命令:" << endl;
    cout << "  -time         获取服务器时间" << endl;
    cout << "  -test [丢包率] 测试停等协议 (丢包率0.0-1.0,默认0.2)" << endl;
    cout << "  -quit         退出程序" << endl;
    cout << "  其他文本      回显测试" << endl;
    cout << "======================================\n" << endl;

    // 主循环
    while (true) {
        cout << "\n请输入命令: ";
        string input;
        getline(cin, input);

        if (input.empty()) continue;

        // 解析命令
        if (input == "-time") {
            // 请求时间
            sendto(clientSocket, input.c_str(), input.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            char recvBuffer[BUFFER_SIZE];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvLen = recvfrom(clientSocket, recvBuffer, BUFFER_SIZE, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[服务器时间] " << recvBuffer << endl;
            }
        }
        else if (input == "-quit") {
            // 退出
            sendto(clientSocket, input.c_str(), input.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            char recvBuffer[BUFFER_SIZE];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvLen = recvfrom(clientSocket, recvBuffer, BUFFER_SIZE, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[服务器] " << recvBuffer << endl;
            }

            break;
        }
        else if (input.substr(0, 5) == "-test") {
            // 测试停等协议
            lossRate = 0.2f; // 默认丢包率

            // 解析丢包率参数
            if (input.length() > 6) {
                try {
                    lossRate = stof(input.substr(6));
                    if (lossRate < 0.0f) lossRate = 0.0f;
                    if (lossRate > 1.0f) lossRate = 1.0f;
                } catch (...) {
                    cout << "[警告] 无效的丢包率, 使用默认值0.2" << endl;
                    lossRate = 0.2f;
                }
            }

            cout << "\n[开始] 停等协议测试" << endl;
            cout << "模拟丢包率: " << (lossRate * 100) << "%" << endl;
            cout << "======================================" << endl;

            // 发送测试命令
            string testCmd = "-test";
            sendto(clientSocket, testCmd.c_str(), testCmd.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            // 接收数据
            unsigned char expectedSeq = 0;
            int receivedCount = 0;

            while (true) {
                DataFrame frame;
                sockaddr_in fromAddr;
                int fromLen = sizeof(fromAddr);

                // 设置接收超时
                DWORD timeout = 10000; // 10秒
                setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
                          (char*)&timeout, sizeof(timeout));

                int recvLen = recvfrom(clientSocket, (char*)&frame, sizeof(DataFrame), 0,
                                      (sockaddr*)&fromAddr, &fromLen);

                if (recvLen > 0) {
                    cout << "\n[接收] Seq=" << (int)frame.seq
                         << ", 数据=\"" << frame.data << "\"" << endl;

                    // 模拟丢包
                    if (simulateLoss()) {
                        cout << "[模拟] 数据包丢失! 不发送ACK" << endl;
                        continue;
                    }

                    // 检查序列号
                    if (frame.seq == expectedSeq) {
                        cout << "[正确] 序列号匹配, 发送ACK=" << (int)expectedSeq << endl;

                        // 发送ACK
                        AckFrame ackFrame;
                        ackFrame.ack = expectedSeq;
                        sendto(clientSocket, (char*)&ackFrame, sizeof(AckFrame), 0,
                              (sockaddr*)&serverAddr, sizeof(serverAddr));

                        receivedCount++;

                        // 更新期望的序列号
                        expectedSeq = 1 - expectedSeq;

                        // 检查是否为最后一帧
                        if (frame.flag == 1) {
                            cout << "\n[完成] 收到结束标志, 传输完成!" << endl;
                            cout << "总共接收: " << receivedCount << " 个数据包" << endl;
                            break;
                        }
                    } else {
                        cout << "[错误] 序列号不匹配! 期望=" << (int)expectedSeq
                             << ", 实际=" << (int)frame.seq << endl;
                        cout << "[重发] 重发上一个ACK=" << (int)(1 - expectedSeq) << endl;

                        // 重发上一个ACK
                        AckFrame ackFrame;
                        ackFrame.ack = 1 - expectedSeq;
                        sendto(clientSocket, (char*)&ackFrame, sizeof(AckFrame), 0,
                              (sockaddr*)&serverAddr, sizeof(serverAddr));
                    }
                } else if (recvLen == SOCKET_ERROR) {
                    int error = WSAGetLastError();
                    if (error == WSAETIMEDOUT) {
                        cout << "\n[超时] 未收到更多数据, 测试结束" << endl;
                        break;
                    } else {
                        cout << "[错误] 接收失败: " << error << endl;
                        break;
                    }
                }
            }

            cout << "======================================" << endl;
        }
        else {
            // 回显测试
            sendto(clientSocket, input.c_str(), input.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            char recvBuffer[BUFFER_SIZE];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvLen = recvfrom(clientSocket, recvBuffer, BUFFER_SIZE, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[回显] " << recvBuffer << endl;
            }
        }
    }

    // 清理
    closesocket(clientSocket);
    WSACleanup();

    cout << "\n客户端已关闭" << endl;

    return 0;
}
