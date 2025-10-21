#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <map>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 客户端配置
#define SERVER_PORT 12340
#define SERVER_IP "127.0.0.1"
#define BUFFER_LENGTH 1026
#define SEQ_SIZE 20          // 序列号空间大小
#define RECV_WIND_SIZE 10    // 接收窗口大小

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

// 接收方缓冲区
struct RecvBuffer {
    DataFrame frame;         // 数据帧
    bool received;           // 是否已接收
};

// 全局变量
float packetLossRate = 0.0f;  // 数据包丢失率
float ackLossRate = 0.0f;     // ACK丢失率
map<int, RecvBuffer> recvWindow;  // 接收窗口缓冲区
int recvBase;                     // 接收窗口基序号

// 模拟丢包
bool simulateLoss(float lossRate) {
    if (lossRate <= 0.0f) return false;
    float random = (float)rand() / RAND_MAX;
    return random < lossRate;
}

// 判断序列号是否在窗口内
bool inWindow(int seq, int base, int windowSize) {
    // 计算序列号到base的距离（环形空间）
    int dist = (seq + SEQ_SIZE - base) % SEQ_SIZE;
    // 如果距离小于窗口大小，说明在窗口内
    return dist < windowSize;
}

// 判断序列号是否在窗口之前（已交付）
bool beforeWindow(int seq, int base) {
    // 计算序列号到base的距离
    int dist = (seq + SEQ_SIZE - base) % SEQ_SIZE;
    // 如果距离 >= SEQ_SIZE/2，说明seq在base之前（已交付的旧包）
    return dist >= SEQ_SIZE / 2;
}

// 打印接收窗口状态
void printRecvWindow() {
    cout << "  [接收窗口] recvBase=" << recvBase << ", 范围: [" 
         << recvBase << ", " << ((recvBase + RECV_WIND_SIZE - 1) % SEQ_SIZE) << "]" << endl;
    cout << "  [";
    for (int i = 0; i < RECV_WIND_SIZE; i++) {
        int seq = (recvBase + i) % SEQ_SIZE;
        if (recvWindow.find(seq) != recvWindow.end() && recvWindow[seq].received) {
            cout << "√";
        } else {
            cout << "×";
        }
    }
    cout << "]" << endl;
}

// 发送ACK
void sendAck(SOCKET sockClient, sockaddr_in& addrServer, unsigned char seq) {
    AckFrame ackFrame;
    ackFrame.ack = seq;
    ackFrame.flag = 0;

    // 模拟ACK丢失
    if (simulateLoss(ackLossRate)) {
        cout << "  [丢失] 模拟ACK=" << (int)seq << "丢失!" << endl;
    } else {
        sendto(sockClient, (char*)&ackFrame, sizeof(AckFrame), 0,
              (sockaddr*)&addrServer, sizeof(addrServer));
        cout << "  [发送] ACK=" << (int)seq << endl;
    }
}

// 交付数据给应用层
int deliverData() {
    int deliveredCount = 0;
    bool isLastDelivered = false;
    
    // 按序交付所有连续的已接收数据
    while (recvWindow.find(recvBase % SEQ_SIZE) != recvWindow.end() && 
           recvWindow[recvBase % SEQ_SIZE].received) {
        
        DataFrame& frame = recvWindow[recvBase % SEQ_SIZE].frame;
        cout << "  [交付] Seq=" << (recvBase % SEQ_SIZE) 
             << ", 数据=\"" << frame.data << "\"";
        
        // 检查是否为最后一帧
        if (frame.flag == 1) {
            cout << " [最后一帧]";
            isLastDelivered = true;
        }
        cout << endl;
        
        // 从缓冲区移除
        recvWindow.erase(recvBase % SEQ_SIZE);
        recvBase++;
        deliveredCount++;
    }
    
    if (deliveredCount > 0) {
        cout << "  [窗口滑动] 新的recvBase=" << recvBase << endl;
    }
    
    // 如果交付了最后一帧，返回负数标记
    return isLastDelivered ? -deliveredCount : deliveredCount;
}

int main() {
    srand((unsigned int)time(NULL));

    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup失败" << endl;
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
    cout << "SR协议客户端" << endl;
    cout << "服务器地址: " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "接收窗口大小: " << RECV_WIND_SIZE << endl;
    cout << "======================================" << endl;
    cout << "\n可用命令:" << endl;
    cout << "  -time              获取服务器时间" << endl;
    cout << "  -testsr [X] [Y]    测试SR协议" << endl;
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
        else if (input.substr(0, 7) == "-testsr") {
            // 测试SR协议
            packetLossRate = 0.2f;
            ackLossRate = 0.2f;

            // 解析参数
            size_t pos = 7;
            if (input.length() > pos) {
                while (pos < input.length() && input[pos] == ' ') pos++;

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

                    while (pos < input.length() && input[pos] == ' ') pos++;

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

            cout << "\n[开始] SR协议测试" << endl;
            cout << "数据包丢失率: " << (packetLossRate * 100) << "%" << endl;
            cout << "ACK丢失率: " << (ackLossRate * 100) << "%" << endl;
            cout << "======================================" << endl;

            // 发送测试命令
            string testCmd = "-testsr";
            sendto(sockClient, testCmd.c_str(), testCmd.length(), 0,
                  (sockaddr*)&addrServer, sizeof(addrServer));

            // 初始化接收状态
            recvWindow.clear();
            recvBase = 0;
            int receivedCount = 0;
            int deliveredCount = 0;
            int lostPacketCount = 0;
            int lostAckCount = 0;
            int duplicateCount = 0;

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

                    // 检查是否在接收窗口内
                    if (inWindow(frame.seq, recvBase % SEQ_SIZE, RECV_WIND_SIZE)) {
                        // 在窗口内
                        if (recvWindow.find(frame.seq) != recvWindow.end() && 
                            recvWindow[frame.seq].received) {
                            // 重复包
                            cout << "  [重复] 已接收过的包，重新发送ACK" << endl;
                            duplicateCount++;
                        } else {
                            // 新包，缓存
                            recvWindow[frame.seq].frame = frame;
                            recvWindow[frame.seq].received = true;
                            receivedCount++;
                            cout << "  [缓存] 存入接收缓冲区" << endl;
                        }

                        // 无论新包还是重复包，都发送ACK
                        sendAck(sockClient, addrServer, frame.seq);
                        if (simulateLoss(ackLossRate)) {
                            lostAckCount++;
                        }

                        // 尝试交付数据
                        int delivered = deliverData();
                        if (delivered < 0) {
                            // 负数表示已交付最后一帧
                            deliveredCount += (-delivered);
                            cout << "\n[完成] 收到并交付最后一个数据包!" << endl;
                            cout << "======================================" << endl;
                            cout << "接收统计:" << endl;
                            cout << "  成功接收: " << receivedCount << " 个数据包" << endl;
                            cout << "  成功交付: " << deliveredCount << " 个数据包" << endl;
                            cout << "  重复接收: " << duplicateCount << " 次" << endl;
                            cout << "  丢弃数据包: " << lostPacketCount << " 个" << endl;
                            cout << "  丢失ACK: " << lostAckCount << " 个" << endl;
                            cout << "======================================" << endl;
                            break;
                        } else {
                            deliveredCount += delivered;
                        }

                        printRecvWindow();
                    } else if (beforeWindow(frame.seq, recvBase % SEQ_SIZE)) {
                        // 在窗口之前（已交付的旧包）
                        cout << "  [过期] 窗口之前的包，重新发送ACK" << endl;
                        sendAck(sockClient, addrServer, frame.seq);
                        if (simulateLoss(ackLossRate)) {
                            lostAckCount++;
                        }
                    } else {
                        // 超出窗口范围（超前）
                        cout << "  [超前] 超出窗口范围，丢弃" << endl;
                    }
                } else if (recvLen == SOCKET_ERROR) {
                    int error = WSAGetLastError();
                    if (error == WSAETIMEDOUT) {
                        cout << "\n[超时] 未收到更多数据,测试结束" << endl;
                        cout << "======================================" << endl;
                        cout << "接收统计:" << endl;
                        cout << "  成功接收: " << receivedCount << " 个数据包" << endl;
                        cout << "  成功交付: " << deliveredCount << " 个数据包" << endl;
                        cout << "  重复接收: " << duplicateCount << " 次" << endl;
                        cout << "  丢弃数据包: " << lostPacketCount << " 个" << endl;
                        cout << "  丢失ACK: " << lostAckCount << " 个" << endl;
                        cout << "  缓冲区剩余: " << recvWindow.size() << " 个" << endl;
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

    closesocket(sockClient);
    WSACleanup();
    cout << "\n客户端已关闭" << endl;

    return 0;
}
