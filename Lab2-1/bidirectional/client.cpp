#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define SERVER_PORT 12340
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define TIMEOUT_MS 2000

// 数据帧结构
struct DataFrame {
    unsigned char seq;
    char data[BUFFER_SIZE];
    unsigned char flag; // 0=数据, 1=结束, 2=ACK
};

// 全局变量
unsigned char sendSeq = 0;
unsigned char recvSeq = 0;
float lossRate = 0.2f;

// 模拟丢包
bool simulateLoss() {
    if (lossRate <= 0.0f) return false;
    return ((float)rand() / RAND_MAX) < lossRate;
}

// 发送数据帧并等待ACK
bool sendDataFrame(SOCKET sock, sockaddr_in& serverAddr, const char* data, bool isEnd = false) {
    DataFrame frame;
    frame.seq = sendSeq;
    strcpy_s(frame.data, data);
    frame.flag = isEnd ? 1 : 0;

    int attempts = 0;
    const int MAX_ATTEMPTS = 5;

    while (attempts < MAX_ATTEMPTS) {
        // 发送数据帧
        sendto(sock, (char*)&frame, sizeof(DataFrame), 0,
               (sockaddr*)&serverAddr, sizeof(serverAddr));

        cout << "[发送->服务器] Seq=" << (int)sendSeq
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
                        sendSeq = 1 - sendSeq;

                        // 恢复非阻塞
                        timeout = 100;
                        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

                        return true;
                    } else {
                        cout << "[错误] ACK序列号不匹配，期望=" << (int)sendSeq
                             << ", 收到=" << (int)ackFrame.seq << endl;
                    }
                } else {
                    // 收到非ACK帧，可能是服务器发来的数据，忽略
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
    DWORD timeout = 100;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    return false;
}

// 发送ACK，返回是否成功发送
bool sendAck(SOCKET sock, sockaddr_in& serverAddr, unsigned char ackSeq) {
    // 模拟ACK丢失
    if (simulateLoss()) {
        cout << "[模拟] ACK丢失!" << endl;
        return false;  // ACK丢失，返回false
    }

    DataFrame ackFrame;
    ackFrame.seq = ackSeq;
    ackFrame.flag = 2;
    strcpy_s(ackFrame.data, "ACK");

    sendto(sock, (char*)&ackFrame, sizeof(DataFrame), 0,
           (sockaddr*)&serverAddr, sizeof(serverAddr));

    cout << "[发送ACK->服务器] ACK=" << (int)ackSeq << endl;
    return true;  // ACK成功发送，返回true
}

int main() {
    srand((unsigned int)time(NULL));

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup失败" << endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        cout << "创建socket失败" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    // 设置非阻塞接收
    DWORD timeout = 100;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    cout << "========================================" << endl;
    cout << "双向停等协议客户端" << endl;
    cout << "服务器: " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "支持双向数据传输" << endl;
    cout << "========================================" << endl;
    cout << "\n命令:" << endl;
    cout << "  -time              获取服务器时间" << endl;
    cout << "  -send              客户端向服务器发送数据" << endl;
    cout << "  -recv [丢包率]      接收服务器发送的数据" << endl;
    cout << "  -both [丢包率]      双向传输测试" << endl;
    cout << "  -quit              退出" << endl;
    cout << "\n说明:" << endl;
    cout << "  - -send: 服务器ACK丢包率由server.cpp配置" << endl;
    cout << "  - -recv: 丢包率控制客户端ACK丢失概率" << endl;
    cout << "  - -both: 丢包率仅影响阶段2的客户端ACK" << endl;
    cout << "========================================\n" << endl;

    while (true) {
        cout << "\n请输入命令: ";
        string input;
        getline(cin, input);

        if (input.empty()) continue;

        if (input == "-time") {
            sendto(clientSocket, input.c_str(), input.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            char recvBuffer[BUFFER_SIZE];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            timeout = 2000;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            int recvLen = recvfrom(clientSocket, recvBuffer, BUFFER_SIZE, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            timeout = 100;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[服务器时间] " << recvBuffer << endl;
            }
        }
        else if (input == "-quit") {
            sendto(clientSocket, input.c_str(), input.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            char recvBuffer[BUFFER_SIZE];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            timeout = 2000;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            int recvLen = recvfrom(clientSocket, recvBuffer, BUFFER_SIZE, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[服务器] " << recvBuffer << endl;
            }

            break;
        }
        else if (input == "-send") {
            // 客户端向服务器发送数据（服务器端控制ACK丢包率）
            cout << "\n[双向传输测试] 客户端->服务器" << endl;
            cout << "注意: ACK丢包率由服务器端控制" << endl;
            cout << "======================================" << endl;

            // 先通知服务器准备接收数据
            string cmd = "-recv";
            sendto(clientSocket, cmd.c_str(), cmd.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            cout << "[通知] 已发送准备命令给服务器..." << endl;

            // 等待服务器响应
            char recvBuffer[BUFFER_SIZE];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            timeout = 2000;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            int recvLen = recvfrom(clientSocket, recvBuffer, BUFFER_SIZE, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[服务器响应] " << recvBuffer << endl;
            } else {
                cout << "[警告] 未收到服务器响应，继续发送..." << endl;
            }

            // 恢复短超时
            timeout = 100;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            // 短暂延迟，确保服务器准备好
            Sleep(100);

            const char* testData[] = {
                "客户端消息1: 你好服务器",
                "客户端消息2: 双向通信",
                "客户端消息3: 测试数据",
                "客户端消息4: 传输完成"
            };

            sendSeq = 0;

            for (int i = 0; i < 4; i++) {
                bool isLast = (i == 3);
                if (!sendDataFrame(clientSocket, serverAddr, testData[i], isLast)) {
                    cout << "[失败] 数据发送失败" << endl;
                    break;
                }
            }

            cout << "======================================" << endl;
            cout << "[完成] 客户端数据发送完毕" << endl;
        }
        else if (input.substr(0, 5) == "-recv") {
            // 接收服务器发送的数据
            lossRate = 0.2f;

            if (input.length() > 6) {
                try {
                    lossRate = stof(input.substr(6));
                    if (lossRate < 0.0f) lossRate = 0.0f;
                    if (lossRate > 1.0f) lossRate = 1.0f;
                } catch (...) {
                    lossRate = 0.2f;
                }
            }

            cout << "\n[双向传输测试] 服务器->客户端" << endl;
            cout << "ACK丢失率: " << (lossRate * 100) << "%" << endl;
            cout << "======================================" << endl;

            // 请求服务器发送数据
            string cmd = "-send";
            sendto(clientSocket, cmd.c_str(), cmd.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            recvSeq = 0;
            int receivedCount = 0;

            // 设置超时
            timeout = 10000;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
                      (char*)&timeout, sizeof(timeout));

            while (true) {
                DataFrame frame;
                sockaddr_in fromAddr;
                int fromLen = sizeof(fromAddr);

                int recvLen = recvfrom(clientSocket, (char*)&frame, sizeof(DataFrame), 0,
                                      (sockaddr*)&fromAddr, &fromLen);

                if (recvLen > 0 && frame.flag != 2) {
                    cout << "\n[接收<-服务器] Seq=" << (int)frame.seq
                         << ", 数据=\"" << frame.data << "\"" << endl;

                    // 模拟数据包丢失
                    if (simulateLoss()) {
                        cout << "[模拟] 数据包丢失!" << endl;
                        continue;
                    }

                    if (frame.seq == recvSeq) {
                        cout << "[接受] 序列号正确" << endl;

                        // 发送ACK，只有成功发送才处理数据和切换序列号
                        if (sendAck(clientSocket, serverAddr, recvSeq)) {
                            receivedCount++;
                            recvSeq = 1 - recvSeq;

                            if (frame.flag == 1) {
                                cout << "\n[完成] 收到结束帧" << endl;
                                cout << "总共接收: " << receivedCount << " 个数据包" << endl;
                                break;
                            }
                        }
                        // 如果ACK丢失，不切换序列号，等待服务器重传
                    } else {
                        cout << "[错误] 序列号不匹配" << endl;
                        sendAck(clientSocket, serverAddr, 1 - recvSeq);
                    }
                } else if (recvLen == SOCKET_ERROR) {
                    if (WSAGetLastError() == WSAETIMEDOUT) {
                        cout << "\n[超时] 接收结束" << endl;
                        break;
                    }
                }
            }

            timeout = 100;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            cout << "======================================" << endl;
        }
        else if (input.substr(0, 5) == "-both") {
            // 双向测试
            lossRate = 0.2f;

            if (input.length() > 6) {
                try {
                    lossRate = stof(input.substr(6));
                    if (lossRate < 0.0f) lossRate = 0.0f;
                    if (lossRate > 1.0f) lossRate = 1.0f;
                } catch (...) {
                    lossRate = 0.2f;
                }
            }

            cout << "\n[双向传输综合测试]" << endl;
            cout << "客户端ACK丢包率（阶段2）: " << (lossRate * 100) << "%" << endl;
            cout << "服务器ACK丢包率（阶段1）: 由服务器配置控制" << endl;
            cout << "========================================" << endl;

            // 先发送
            cout << "\n阶段1: 客户端->服务器" << endl;
            cout << "----------------------------------------" << endl;

            // 先通知服务器准备接收数据
            string notifyCmd = "-recv";
            sendto(clientSocket, notifyCmd.c_str(), notifyCmd.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            cout << "[通知] 已发送准备命令给服务器..." << endl;

            // 等待服务器响应
            char recvBuffer[BUFFER_SIZE];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            timeout = 2000;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            int recvLen = recvfrom(clientSocket, recvBuffer, BUFFER_SIZE, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[服务器响应] " << recvBuffer << endl;
            }

            // 恢复短超时
            timeout = 100;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            Sleep(100); // 短暂延迟

            const char* clientData[] = {
                "客户端: Hello Server!",
                "客户端: 双向通信测试"
            };

            sendSeq = 0;
            for (int i = 0; i < 2; i++) {
                sendDataFrame(clientSocket, serverAddr, clientData[i], i == 1);
            }

            Sleep(1000); // 等待1秒

            // 再接收
            cout << "\n阶段2: 服务器->客户端" << endl;
            cout << "----------------------------------------" << endl;

            string cmd = "-send";
            sendto(clientSocket, cmd.c_str(), cmd.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            recvSeq = 0;

            timeout = 10000;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
                      (char*)&timeout, sizeof(timeout));

            while (true) {
                DataFrame frame;
                sockaddr_in fromAddr;
                int fromLen = sizeof(fromAddr);

                int recvLen = recvfrom(clientSocket, (char*)&frame, sizeof(DataFrame), 0,
                                      (sockaddr*)&fromAddr, &fromLen);

                if (recvLen > 0 && frame.flag != 2) {
                    if (!simulateLoss() && frame.seq == recvSeq) {
                        cout << "[接收] " << frame.data << endl;

                        // 发送ACK，只有成功发送才切换序列号
                        if (sendAck(clientSocket, serverAddr, recvSeq)) {
                            recvSeq = 1 - recvSeq;
                        }

                        if (frame.flag == 1) break;
                    }
                } else if (recvLen == SOCKET_ERROR && WSAGetLastError() == WSAETIMEDOUT) {
                    break;
                }
            }

            timeout = 100;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            cout << "\n========================================" << endl;
            cout << "[完成] 双向传输测试完成!" << endl;
            cout << "========================================" << endl;
        }
        else {
            sendto(clientSocket, input.c_str(), input.length(), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            char recvBuffer[BUFFER_SIZE];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            timeout = 2000;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            int recvLen = recvfrom(clientSocket, recvBuffer, BUFFER_SIZE, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            timeout = 100;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[回显] " << recvBuffer << endl;
            }
        }
    }

    closesocket(clientSocket);
    WSACleanup();

    cout << "\n客户端已关闭" << endl;

    return 0;
}
