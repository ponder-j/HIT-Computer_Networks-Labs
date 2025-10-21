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

#define SERVER_PORT 12340
#define SERVER_IP "127.0.0.1"
#define BUFFER_LENGTH 1026
#define SEQ_SIZE 20
#define RECV_WIND_SIZE 10

// 数据帧结构
struct DataFrame {
    unsigned char seq;
    char data[1024];
    unsigned char flag; // 0=数据, 1=结束, 2=ACK
};

// 全局变量
float packetLossRate = 0.0f;
float ackLossRate = 0.0f;

BOOL recvAck[SEQ_SIZE];     // 已接收标记
map<int, DataFrame> recvBuffer; // 接收缓冲区
int recvBase = 0;           // 接收窗口基序号

// 模拟丢包
bool simulateLoss(float lossRate) {
    if (lossRate <= 0.0f) return false;
    return ((float)rand() / RAND_MAX) < lossRate;
}

// 判断在接收窗口内
bool inRecvWindow(int seq) {
    if (recvBase <= recvBase + RECV_WIND_SIZE - 1) {
        return seq >= recvBase && seq < recvBase + RECV_WIND_SIZE;
    } else {
        return seq >= recvBase || seq < (recvBase + RECV_WIND_SIZE) % SEQ_SIZE;
    }
}

// 打印接收窗口
void printRecvWindow() {
    cout << "  窗口[";
    for (int i = 0; i < SEQ_SIZE; i++) {
        if (i == recvBase) cout << "|";
        if (inRecvWindow(i)) {
            cout << (recvAck[i] ? "√" : "·");
        } else {
            cout << " ";
        }
        if (i == (recvBase + RECV_WIND_SIZE - 1) % SEQ_SIZE) cout << "|";
    }
    cout << "] base=" << recvBase << endl;
}

int main() {
    srand((unsigned int)time(NULL));

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup失败" << endl;
        return 1;
    }

    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockClient == INVALID_SOCKET) {
        cout << "创建socket失败" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in addrServer;
    addrServer.sin_family = AF_INET;
    addrServer.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);

    cout << "========================================" << endl;
    cout << "SR协议客户端(Selective Repeat)" << endl;
    cout << "服务器: " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "接收窗口: " << RECV_WIND_SIZE << endl;
    cout << "========================================" << endl;
    cout << "\n命令:" << endl;
    cout << "  -time              获取服务器时间" << endl;
    cout << "  -testsr [X] [Y]    测试SR协议" << endl;
    cout << "       X: 数据包丢失率(默认0.2)" << endl;
    cout << "       Y: ACK丢失率(默认0.2)" << endl;
    cout << "  -quit              退出" << endl;
    cout << "========================================\n" << endl;

    while (true) {
        cout << "\n请输入命令: ";
        string input;
        getline(cin, input);

        if (input.empty()) continue;

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
                    } catch (...) {}

                    while (pos < input.length() && input[pos] == ' ') pos++;

                    if (pos < input.length()) {
                        try {
                            ackLossRate = stof(input.substr(pos));
                            if (ackLossRate < 0.0f) ackLossRate = 0.0f;
                            if (ackLossRate > 1.0f) ackLossRate = 1.0f;
                        } catch (...) {}
                    }
                }
            }

            cout << "\n[开始] SR协议测试" << endl;
            cout << "数据包丢失率: " << (packetLossRate * 100) << "%" << endl;
            cout << "ACK丢失率: " << (ackLossRate * 100) << "%" << endl;
            cout << "========================================" << endl;

            // 发送测试命令
            string testCmd = "-testsr";
            sendto(sockClient, testCmd.c_str(), testCmd.length(), 0,
                  (sockaddr*)&addrServer, sizeof(addrServer));

            // 初始化接收状态
            for (int i = 0; i < SEQ_SIZE; i++) {
                recvAck[i] = FALSE;
            }
            recvBase = 0;
            recvBuffer.clear();

            int receivedCount = 0;
            int lostPacketCount = 0;
            int lostAckCount = 0;
            int outOfOrderCount = 0;

            DWORD timeout = 15000;
            setsockopt(sockClient, SOL_SOCKET, SO_RCVTIMEO,
                      (char*)&timeout, sizeof(timeout));

            printRecvWindow();

            while (true) {
                DataFrame frame;
                sockaddr_in fromAddr;
                int fromLen = sizeof(fromAddr);

                int recvLen = recvfrom(sockClient, (char*)&frame, sizeof(DataFrame), 0,
                                      (sockaddr*)&fromAddr, &fromLen);

                if (recvLen > 0 && frame.flag != 2) {
                    cout << "\n[接收] Seq=" << (int)frame.seq
                         << ", 数据=\"" << frame.data << "\"";

                    // 模拟数据包丢失
                    if (simulateLoss(packetLossRate)) {
                        cout << " -> [丢弃]模拟丢包!" << endl;
                        lostPacketCount++;
                        continue;
                    }

                    cout << endl;

                    // SR协议:检查是否在接收窗口内
                    if (inRecvWindow(frame.seq)) {
                        if (!recvAck[frame.seq]) {
                            // 新数据包
                            recvBuffer[frame.seq] = frame;
                            recvAck[frame.seq] = TRUE;

                            if (frame.seq != recvBase) {
                                cout << "[乱序] Seq=" << (int)frame.seq
                                     << " 不是期望的 " << recvBase << ", 缓存!" << endl;
                                outOfOrderCount++;
                            } else {
                                cout << "[按序] Seq=" << (int)frame.seq << endl;
                            }

                            receivedCount++;

                            // SR协议:立即发送ACK
                            DataFrame ackFrame;
                            ackFrame.seq = frame.seq;
                            ackFrame.flag = 2;

                            if (simulateLoss(ackLossRate)) {
                                cout << "[丢失] ACK=" << (int)frame.seq << " 丢失!" << endl;
                                lostAckCount++;
                            } else {
                                sendto(sockClient, (char*)&ackFrame, sizeof(DataFrame), 0,
                                      (sockaddr*)&addrServer, sizeof(addrServer));
                                cout << "[发送ACK] ACK=" << (int)frame.seq << endl;
                            }

                            // 如果是recvBase,交付连续数据
                            if (frame.seq == recvBase) {
                                cout << "[交付] 开始交付连续数据:" << endl;
                                while (recvAck[recvBase]) {
                                    if (recvBuffer.find(recvBase) != recvBuffer.end()) {
                                        cout << "  -> Seq=" << recvBase
                                             << ", 数据=\"" << recvBuffer[recvBase].data << "\"" << endl;

                                        // 检查结束
                                        if (recvBuffer[recvBase].flag == 1) {
                                            cout << "\n========================================" << endl;
                                            cout << "[完成] 接收完成!" << endl;
                                            cout << "成功接收: " << receivedCount << " 个" << endl;
                                            cout << "丢弃数据包: " << lostPacketCount << " 个" << endl;
                                            cout << "丢失ACK: " << lostAckCount << " 个" << endl;
                                            cout << "乱序包: " << outOfOrderCount << " 个" << endl;
                                            cout << "========================================" << endl;
                                            goto test_complete;
                                        }

                                        recvBuffer.erase(recvBase);
                                    }
                                    recvAck[recvBase] = FALSE;
                                    recvBase = (recvBase + 1) % SEQ_SIZE;
                                }
                                cout << "  新recvBase=" << recvBase << endl;
                            }

                            printRecvWindow();
                        } else {
                            // 重复包,重发ACK
                            cout << "[重复] Seq=" << (int)frame.seq << ", 重发ACK" << endl;

                            DataFrame ackFrame;
                            ackFrame.seq = frame.seq;
                            ackFrame.flag = 2;

                            if (!simulateLoss(ackLossRate)) {
                                sendto(sockClient, (char*)&ackFrame, sizeof(DataFrame), 0,
                                      (sockaddr*)&addrServer, sizeof(addrServer));
                            }
                        }
                    } else {
                        cout << "[丢弃] Seq=" << (int)frame.seq << " 不在窗口内" << endl;
                    }
                } else if (recvLen == SOCKET_ERROR) {
                    if (WSAGetLastError() == WSAETIMEDOUT) {
                        cout << "\n[超时] 测试结束" << endl;
                        cout << "========================================" << endl;
                        cout << "成功接收: " << receivedCount << " 个" << endl;
                        cout << "丢弃数据包: " << lostPacketCount << " 个" << endl;
                        cout << "丢失ACK: " << lostAckCount << " 个" << endl;
                        cout << "乱序包: " << outOfOrderCount << " 个" << endl;
                        cout << "========================================" << endl;
                        break;
                    }
                }
            }

            test_complete:;
        }
        else {
            cout << "[错误] 未知命令" << endl;
        }
    }

    closesocket(sockClient);
    WSACleanup();

    cout << "\n客户端已关闭" << endl;

    return 0;
}
