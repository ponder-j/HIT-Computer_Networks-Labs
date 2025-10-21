#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <ctime>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 服务器配置
#define SERVER_PORT 12340
#define BUFFER_SIZE 1024
#define TIMEOUT_MS 2000  // 超时时间2秒

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

// 获取当前时间字符串
string getCurrentTime() {
    time_t now = time(0);
    char* dt = ctime(&now);
    string timeStr(dt);
    timeStr.pop_back(); // 移除换行符
    return timeStr;
}

// 发送数据帧并等待ACK
bool sendDataFrame(SOCKET sock, sockaddr_in& clientAddr, DataFrame& frame, int expectedAck) {
    char buffer[sizeof(DataFrame)];
    memcpy(buffer, &frame, sizeof(DataFrame));

    int attempts = 0;
    const int MAX_ATTEMPTS = 5;

    while (attempts < MAX_ATTEMPTS) {
        // 发送数据帧
        int sendResult = sendto(sock, buffer, sizeof(DataFrame), 0,
                               (sockaddr*)&clientAddr, sizeof(clientAddr));

        if (sendResult == SOCKET_ERROR) {
            cout << "[错误] 发送数据失败" << endl;
            return false;
        }

        cout << "[发送] Seq=" << (int)frame.seq << ", 数据大小=" << strlen(frame.data)
             << " 字节, 尝试次数=" << (attempts + 1) << endl;

        // 等待ACK
        sockaddr_in fromAddr;
        int fromLen = sizeof(fromAddr);
        AckFrame ackFrame;

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);

        timeval timeout;
        timeout.tv_sec = TIMEOUT_MS / 1000;
        timeout.tv_usec = (TIMEOUT_MS % 1000) * 1000;

        int selectResult = select(0, &readSet, NULL, NULL, &timeout);

        if (selectResult > 0) {
            // 有数据可读
            int recvLen = recvfrom(sock, (char*)&ackFrame, sizeof(AckFrame), 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                cout << "[接收] ACK=" << (int)ackFrame.ack << endl;

                if (ackFrame.ack == expectedAck) {
                    cout << "[成功] 收到正确ACK" << endl;
                    return true;
                } else {
                    cout << "[警告] ACK序列号不匹配, 期望=" << expectedAck
                         << ", 实际=" << (int)ackFrame.ack << endl;
                }
            }
        } else if (selectResult == 0) {
            // 超时
            cout << "[超时] 未收到ACK, 准备重传..." << endl;
        } else {
            cout << "[错误] select调用失败" << endl;
        }

        attempts++;
    }

    cout << "[失败] 达到最大重传次数" << endl;
    return false;
}

int main() {
    // 初始化Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cout << "WSAStartup失败: " << result << endl;
        return 1;
    }

    // 创建UDP socket
    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        cout << "创建socket失败: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // 绑定地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "绑定失败: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "======================================" << endl;
    cout << "停等协议服务器已启动" << endl;
    cout << "监听端口: " << SERVER_PORT << endl;
    cout << "======================================" << endl;

    // 主循环
    while (true) {
        char recvBuffer[BUFFER_SIZE];
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);

        cout << "\n等待客户端请求..." << endl;

        // 接收客户端请求
        int recvLen = recvfrom(serverSocket, recvBuffer, BUFFER_SIZE, 0,
                              (sockaddr*)&clientAddr, &clientAddrLen);

        if (recvLen > 0) {
            recvBuffer[recvLen] = '\0';
            string command(recvBuffer);

            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);

            cout << "[请求] 来自 " << clientIP << ":" << ntohs(clientAddr.sin_port)
                 << " - " << command << endl;

            // 处理命令
            if (command == "-time") {
                // 返回当前时间
                string timeStr = getCurrentTime();
                sendto(serverSocket, timeStr.c_str(), timeStr.length(), 0,
                      (sockaddr*)&clientAddr, clientAddrLen);
                cout << "[响应] 发送时间: " << timeStr << endl;
            }
            else if (command == "-quit") {
                // 客户端退出
                string goodbye = "Good bye!";
                sendto(serverSocket, goodbye.c_str(), goodbye.length(), 0,
                      (sockaddr*)&clientAddr, clientAddrLen);
                cout << "[响应] 客户端断开连接" << endl;
            }
            else if (command == "-test") {
                // 测试停等协议
                cout << "\n[开始] 停等协议数据传输测试" << endl;
                cout << "======================================" << endl;

                // 准备测试数据
                const char* testData[] = {
                    "这是第一个数据包",
                    "这是第二个数据包",
                    "这是第三个数据包",
                    "停等协议测试数据",
                    "最后一个数据包"
                };
                int dataCount = 5;

                // 使用停等协议发送数据
                unsigned char currentSeq = 0; // 当前序列号 (0或1)
                bool success = true;

                for (int i = 0; i < dataCount; i++) {
                    DataFrame frame;
                    frame.seq = currentSeq;
                    strcpy_s(frame.data, testData[i]);
                    frame.flag = (i == dataCount - 1) ? 1 : 0; // 最后一帧标记为结束

                    cout << "\n--- 发送数据包 " << (i + 1) << "/" << dataCount << " ---" << endl;

                    if (!sendDataFrame(serverSocket, clientAddr, frame, currentSeq)) {
                        cout << "[失败] 数据包发送失败" << endl;
                        success = false;
                        break;
                    }

                    // 切换序列号 (0->1, 1->0)
                    currentSeq = 1 - currentSeq;
                }

                cout << "\n======================================" << endl;
                if (success) {
                    cout << "[完成] 所有数据包已成功发送" << endl;
                } else {
                    cout << "[失败] 数据传输失败" << endl;
                }
                cout << "======================================" << endl;
            }
            else {
                // 回显数据
                sendto(serverSocket, recvBuffer, recvLen, 0,
                      (sockaddr*)&clientAddr, clientAddrLen);
                cout << "[响应] 回显数据: " << command << endl;
            }
        }
    }

    // 清理
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
