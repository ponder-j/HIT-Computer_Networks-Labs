#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <ctime>
#include <vector>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 服务器配置
#define SERVER_PORT 12340
#define BUFFER_LENGTH 1026
#define SEND_WIND_SIZE 10    // 发送窗口大小
#define SEQ_SIZE 20          // 序列号个数(0-19)
#define TIMEOUT_THRESHOLD 50 // 超时阈值

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
BOOL ack[SEQ_SIZE];         // 收到ACK情况
int curSeq;                 // 当前数据包的seq
int curAck;                 // 当前等待确认的ack
int totalSeq;               // 总的数据包数
int totalPacket;            // 需要发送的包总数
vector<DataFrame> packets;  // 数据包缓冲区

// 获取当前时间
void getCurTime(char* ptime) {
    time_t now = time(0);
    strcpy(ptime, ctime(&now));
    ptime[strlen(ptime) - 1] = '\0'; // 移除换行符
}

// 判断当前序列号是否可用
bool seqIsAvailable() {
    int step = (curSeq + SEQ_SIZE - curAck) % SEQ_SIZE;
    return step < SEND_WIND_SIZE;
}

// 打印发送窗口状态
void printWindow() {
    cout << "[窗口状态] curAck=" << curAck << ", curSeq=" << curSeq
         << ", 窗口大小=" << SEND_WIND_SIZE << endl;
    cout << "  [";
    for (int i = 0; i < SEQ_SIZE; i++) {
        if (i == curAck) cout << "|";
        if (i >= curAck && i < (curAck + SEND_WIND_SIZE) % SEQ_SIZE) {
            cout << (ack[i] ? "√" : "×");
        } else {
            cout << " ";
        }
        if (i == (curAck + SEND_WIND_SIZE - 1) % SEQ_SIZE) cout << "|";
    }
    cout << "]" << endl;
}

// 超时处理:重传窗口内所有未确认的包
void timeoutHandler(SOCKET sockServer, sockaddr_in& clientAddr) {
    cout << "\n[超时] 检测到超时,重传窗口内所有数据包..." << endl;

    int start = curAck;
    int end = curSeq;

    for (int i = start; i != end; i = (i + 1) % SEQ_SIZE) {
        if (i < packets.size()) {
            sendto(sockServer, (char*)&packets[i], sizeof(DataFrame), 0,
                  (sockaddr*)&clientAddr, sizeof(clientAddr));
            cout << "[重传] Seq=" << i << ", 数据=\"" << packets[i].data << "\"" << endl;
        }
    }
}

// 处理收到的ACK
void ackHandler(unsigned char c) {
    cout << "[ACK处理] 收到ACK=" << (int)c << endl;

    if (c >= curAck) {
        // 正常情况:累积确认
        for (int i = curAck; i <= c; i++) {
            ack[i] = TRUE;
        }

        // 移动窗口
        while (ack[curAck] && curAck != curSeq) {
            ack[curAck] = FALSE;
            curAck = (curAck + 1) % SEQ_SIZE;
        }

        cout << "[窗口滑动] 新的curAck=" << curAck << endl;
    } else if (c < curAck) {
        // 处理序列号回绕的情况
        if (c + SEQ_SIZE >= curAck) {
            for (int i = curAck; i < SEQ_SIZE; i++) {
                ack[i] = TRUE;
            }
            for (int i = 0; i <= c; i++) {
                ack[i] = TRUE;
            }

            while (ack[curAck] && curAck != curSeq) {
                ack[curAck] = FALSE;
                curAck = (curAck + 1) % SEQ_SIZE;
            }
        }
    }

    printWindow();
}

int main() {
    // 初始化Winsock
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0) {
        cout << "WSAStartup失败: " << err << endl;
        return 1;
    }

    // 创建socket
    SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockServer == INVALID_SOCKET) {
        cout << "创建socket失败" << endl;
        WSACleanup();
        return 1;
    }

    // 设置非阻塞模式
    u_long iMode = 1;
    ioctlsocket(sockServer, FIONBIO, &iMode);

    // 绑定地址
    sockaddr_in addrServer;
    addrServer.sin_family = AF_INET;
    addrServer.sin_addr.s_addr = INADDR_ANY;
    addrServer.sin_port = htons(SERVER_PORT);

    if (bind(sockServer, (sockaddr*)&addrServer, sizeof(addrServer)) == SOCKET_ERROR) {
        cout << "绑定失败: " << WSAGetLastError() << endl;
        closesocket(sockServer);
        WSACleanup();
        return 1;
    }

    cout << "======================================" << endl;
    cout << "GBN协议服务器已启动" << endl;
    cout << "监听端口: " << SERVER_PORT << endl;
    cout << "窗口大小: " << SEND_WIND_SIZE << endl;
    cout << "序列号范围: 0-" << (SEQ_SIZE - 1) << endl;
    cout << "======================================" << endl;

    sockaddr_in addrClient;
    int addrClientLen = sizeof(addrClient);
    char recvBuffer[BUFFER_LENGTH];

    // 主循环
    while (true) {
        int recvLen = recvfrom(sockServer, recvBuffer, BUFFER_LENGTH, 0,
                              (sockaddr*)&addrClient, &addrClientLen);

        if (recvLen > 0) {
            recvBuffer[recvLen] = '\0';
            string command(recvBuffer);

            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addrClient.sin_addr), clientIP, INET_ADDRSTRLEN);

            cout << "\n[接收] 来自 " << clientIP << ":" << ntohs(addrClient.sin_port)
                 << " - " << command << endl;

            // 处理命令
            if (command == "-time") {
                char timeStr[100];
                getCurTime(timeStr);
                sendto(sockServer, timeStr, strlen(timeStr), 0,
                      (sockaddr*)&addrClient, addrClientLen);
                cout << "[响应] 发送时间: " << timeStr << endl;
            }
            else if (command == "-quit") {
                string goodbye = "Good bye!";
                sendto(sockServer, goodbye.c_str(), goodbye.length(), 0,
                      (sockaddr*)&addrClient, addrClientLen);
                cout << "[响应] 客户端断开" << endl;
            }
            else if (command == "-testgbn") {
                cout << "\n[开始] GBN协议测试" << endl;
                cout << "======================================" << endl;

                // 初始化GBN参数
                for (int i = 0; i < SEQ_SIZE; i++) {
                    ack[i] = FALSE;
                }
                curSeq = 0;
                curAck = 0;
                totalSeq = 0;
                packets.clear();

                // 准备测试数据
                const char* testData[] = {
                    "数据包1:GBN协议测试开始",
                    "数据包2:Go-Back-N测试",
                    "数据包3:滑动窗口协议",
                    "数据包4:累积确认机制",
                    "数据包5:超时重传测试",
                    "数据包6:窗口大小为10",
                    "数据包7:序列号0-19",
                    "数据包8:可靠数据传输",
                    "数据包9:网络协议实验",
                    "数据包10:计算机网络",
                    "数据包11:传输层协议",
                    "数据包12:UDP封装",
                    "数据包13:丢包模拟",
                    "数据包14:ACK确认",
                    "数据包15:最后的测试数据"
                };
                totalPacket = 15;

                // 准备所有数据包
                for (int i = 0; i < totalPacket; i++) {
                    DataFrame frame;
                    frame.seq = i % SEQ_SIZE;
                    strcpy(frame.data, testData[i]);
                    frame.flag = (i == totalPacket - 1) ? 1 : 0;
                    packets.push_back(frame);
                }

                cout << "准备发送 " << totalPacket << " 个数据包" << endl;
                printWindow();

                // GBN发送循环
                int timer = 0;
                bool transferComplete = false;

                while (!transferComplete) {
                    // 在窗口允许的范围内发送数据
                    while (seqIsAvailable() && curSeq < totalPacket) {
                        sendto(sockServer, (char*)&packets[curSeq], sizeof(DataFrame), 0,
                              (sockaddr*)&addrClient, addrClientLen);

                        cout << "[发送] Seq=" << curSeq
                             << ", 数据=\"" << packets[curSeq].data << "\"" << endl;

                        curSeq = (curSeq + 1) % SEQ_SIZE;
                        totalSeq++;
                        timer = 0; // 重置计时器
                    }

                    // 尝试接收ACK
                    AckFrame ackFrame;
                    sockaddr_in fromAddr;
                    int fromLen = sizeof(fromAddr);

                    int ackLen = recvfrom(sockServer, (char*)&ackFrame, sizeof(AckFrame), 0,
                                         (sockaddr*)&fromAddr, &fromLen);

                    if (ackLen > 0) {
                        // 收到ACK,处理并重置计时器
                        ackHandler(ackFrame.ack);
                        timer = 0;

                        // 检查是否所有数据都已确认
                        if (curAck == totalPacket % SEQ_SIZE && totalSeq == totalPacket) {
                            transferComplete = true;
                        }
                    } else {
                        // 没有收到ACK,计时器递增
                        timer++;

                        if (timer >= TIMEOUT_THRESHOLD) {
                            // 超时,重传
                            timeoutHandler(sockServer, addrClient);
                            timer = 0;
                        }

                        Sleep(1); // 短暂休眠
                    }
                }

                cout << "\n======================================" << endl;
                cout << "[完成] GBN协议测试完成!" << endl;
                cout << "总共发送: " << totalPacket << " 个数据包" << endl;
                cout << "======================================" << endl;
            }
            else {
                // 回显
                sendto(sockServer, recvBuffer, recvLen, 0,
                      (sockaddr*)&addrClient, addrClientLen);
                cout << "[响应] 回显数据" << endl;
            }
        }

        Sleep(1); // 短暂休眠,避免CPU占用过高
    }

    // 清理
    closesocket(sockServer);
    WSACleanup();

    return 0;
}
