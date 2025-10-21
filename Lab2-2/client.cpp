#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 客户端配置
#define SERVER_PORT 12340
#define SERVER_IP "127.0.0.1"
#define BUFFER_LENGTH 1026
#define SEQ_SIZE 20

// 数据帧结构
struct DataFrame {
    unsigned char seq;       // 序列号
    char data[1024];         // 数据
    unsigned char flag;      // 标志位: 0=普通帧, 1=结束帧
};

// ACK帧结构
struct AckFrame {
    unsigned char ack;       // ACK序列号
    unsigned char flag;      // 标志位
};

// 全局变量
float packetLossRate = 0.0f;  // 数据包丢失率
float ackLossRate = 0.0f;     // ACK丢失率

// 模拟丢包
bool simulateLoss(float lossRate) {
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

    // 创建socket
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockClient == INVALID_SOCKET) {
        cout << "创建socket失败" << endl;
        WSACleanup();
        return 1;
    }

    // 设置服务器地址
    sockaddr_in addrServer;
    addrServer.sin_family = AF_INET;
    addrServer.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);

    cout << "======================================" << endl;
    cout << "GBN协议客户端" << endl;
    cout << "服务器地址: " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "======================================" << endl;
    cout << "\n可用命令:" << endl;
    cout << "  -time              获取服务器时间" << endl;
    cout << "  -testgbn [X] [Y]   测试GBN协议" << endl;
    cout << "                     X: 数据包丢失率(0.0-1.0,默认0.2)" << endl;
    cout << "                     Y: ACK丢失率(0.0-1.0,默认0.2)" << endl;
    cout << "  -quit              退出程序" << endl;
    cout << "  其他文本           回显测试" << endl;
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
            sendto(sockClient, input.c_str(), input.length(), 0,
                  (sockaddr*)&addrServer, sizeof(addrServer));

            char recvBuffer[BUFFER_LENGTH];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvLen = recvfrom(sockClient, recvBuffer, BUFFER_LENGTH, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[服务器时间] " << recvBuffer << endl;
            }
        }
        else if (input == "-quit") {
            // 退出
            sendto(sockClient, input.c_str(), input.length(), 0,
                  (sockaddr*)&addrServer, sizeof(addrServer));

            char recvBuffer[BUFFER_LENGTH];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvLen = recvfrom(sockClient, recvBuffer, BUFFER_LENGTH, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[服务器] " << recvBuffer << endl;
            }

            break;
        }
        else if (input.substr(0, 8) == "-testgbn") {
            // 测试GBN协议
            packetLossRate = 0.2f;
            ackLossRate = 0.2f;

            // 解析参数
            size_t pos = 8;
            if (input.length() > pos) {
                // 跳过空格
                while (pos < input.length() && input[pos] == ' ') pos++;

                // 解析第一个参数
                if (pos < input.length()) {
                    size_t nextSpace = input.find(' ', pos);
                    string param1;
                    if (nextSpace != string::npos) {
                        param1 = input.substr(pos, nextSpace - pos);
                        pos = nextSpace + 1;
                    } else {
                        param1 = input.substr(pos);
                        pos = input.length();
                    }

                    try {
                        packetLossRate = stof(param1);
                        if (packetLossRate < 0.0f) packetLossRate = 0.0f;
                        if (packetLossRate > 1.0f) packetLossRate = 1.0f;
                    } catch (...) {
                        cout << "[警告] 无效的数据包丢失率,使用默认值0.2" << endl;
                    }

                    // 跳过空格
                    while (pos < input.length() && input[pos] == ' ') pos++;

                    // 解析第二个参数
                    if (pos < input.length()) {
                        string param2 = input.substr(pos);
                        try {
                            ackLossRate = stof(param2);
                            if (ackLossRate < 0.0f) ackLossRate = 0.0f;
                            if (ackLossRate > 1.0f) ackLossRate = 1.0f;
                        } catch (...) {
                            cout << "[警告] 无效的ACK丢失率,使用默认值0.2" << endl;
                        }
                    }
                }
            }

            cout << "\n[开始] GBN协议测试" << endl;
            cout << "数据包丢失率: " << (packetLossRate * 100) << "%" << endl;
            cout << "ACK丢失率: " << (ackLossRate * 100) << "%" << endl;
            cout << "======================================" << endl;

            // 发送测试命令
            string testCmd = "-testgbn";
            sendto(sockClient, testCmd.c_str(), testCmd.length(), 0,
                  (sockaddr*)&addrServer, sizeof(addrServer));

            // 接收数据
            int expectedSeq = 0;        // 期望的序列号
            int receivedCount = 0;      // 接收计数
            int lostPacketCount = 0;    // 丢包计数
            int lostAckCount = 0;       // ACK丢失计数

            // 设置接收超时
            DWORD timeout = 15000; // 15秒
            setsockopt(sockClient, SOL_SOCKET, SO_RCVTIMEO,
                      (char*)&timeout, sizeof(timeout));

            while (true) {
                DataFrame frame;
                sockaddr_in fromAddr;
                int fromLen = sizeof(fromAddr);

                int recvLen = recvfrom(sockClient, (char*)&frame, sizeof(DataFrame), 0,
                                      (sockaddr*)&fromAddr, &fromLen);

                if (recvLen > 0) {
                    cout << "\n[接收] Seq=" << (int)frame.seq
                         << ", 数据=\"" << frame.data << "\"";

                    // 模拟数据包丢失
                    if (simulateLoss(packetLossRate)) {
                        cout << " -> [丢弃] 模拟数据包丢失!" << endl;
                        lostPacketCount++;
                        continue;
                    }

                    cout << endl;

                    // GBN协议:只接受期望的序列号
                    if (frame.seq == expectedSeq) {
                        cout << "[接受] 序列号正确,发送ACK=" << (int)expectedSeq << endl;

                        // 发送累积确认ACK
                        AckFrame ackFrame;
                        ackFrame.ack = expectedSeq;
                        ackFrame.flag = 0;

                        // 模拟ACK丢失
                        if (simulateLoss(ackLossRate)) {
                            cout << "[丢失] 模拟ACK丢失!" << endl;
                            lostAckCount++;
                        } else {
                            sendto(sockClient, (char*)&ackFrame, sizeof(AckFrame), 0,
                                  (sockaddr*)&addrServer, sizeof(addrServer));
                            cout << "[发送] ACK=" << (int)expectedSeq << endl;
                        }

                        receivedCount++;
                        expectedSeq = (expectedSeq + 1) % SEQ_SIZE;

                        // 检查是否为最后一帧
                        if (frame.flag == 1) {
                            cout << "\n[完成] 收到结束标志!" << endl;
                            cout << "======================================" << endl;
                            cout << "接收统计:" << endl;
                            cout << "  成功接收: " << receivedCount << " 个数据包" << endl;
                            cout << "  丢弃数据包: " << lostPacketCount << " 个" << endl;
                            cout << "  丢失ACK: " << lostAckCount << " 个" << endl;
                            cout << "======================================" << endl;
                            break;
                        }
                    } else {
                        // 收到非期望序列号,发送上一个ACK(累积确认)
                        int prevAck = (expectedSeq - 1 + SEQ_SIZE) % SEQ_SIZE;
                        cout << "[拒绝] 序列号不匹配! 期望=" << expectedSeq
                             << ", 实际=" << (int)frame.seq << endl;
                        cout << "[重发] 发送上一个ACK=" << prevAck << endl;

                        AckFrame ackFrame;
                        ackFrame.ack = prevAck;
                        ackFrame.flag = 0;

                        // 模拟ACK丢失
                        if (simulateLoss(ackLossRate)) {
                            cout << "[丢失] 模拟ACK丢失!" << endl;
                            lostAckCount++;
                        } else {
                            sendto(sockClient, (char*)&ackFrame, sizeof(AckFrame), 0,
                                  (sockaddr*)&addrServer, sizeof(addrServer));
                        }
                    }
                } else if (recvLen == SOCKET_ERROR) {
                    int error = WSAGetLastError();
                    if (error == WSAETIMEDOUT) {
                        cout << "\n[超时] 未收到更多数据,测试结束" << endl;
                        cout << "======================================" << endl;
                        cout << "接收统计:" << endl;
                        cout << "  成功接收: " << receivedCount << " 个数据包" << endl;
                        cout << "  丢弃数据包: " << lostPacketCount << " 个" << endl;
                        cout << "  丢失ACK: " << lostAckCount << " 个" << endl;
                        cout << "======================================" << endl;
                        break;
                    } else {
                        cout << "[错误] 接收失败: " << error << endl;
                        break;
                    }
                }
            }
        }
        else {
            // 回显测试
            sendto(sockClient, input.c_str(), input.length(), 0,
                  (sockaddr*)&addrServer, sizeof(addrServer));

            char recvBuffer[BUFFER_LENGTH];
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvLen = recvfrom(sockClient, recvBuffer, BUFFER_LENGTH, 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                recvBuffer[recvLen] = '\0';
                cout << "[回显] " << recvBuffer << endl;
            }
        }
    }

    // 清理
    closesocket(sockClient);
    WSACleanup();

    cout << "\n客户端已关闭" << endl;

    return 0;
}
