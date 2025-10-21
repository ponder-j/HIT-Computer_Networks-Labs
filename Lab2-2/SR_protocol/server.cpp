#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <ctime>
#include <vector>
#include <map>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define SERVER_PORT 12340
#define BUFFER_LENGTH 1026
#define SEND_WIND_SIZE 10    // 发送窗口大小
#define RECV_WIND_SIZE 10    // 接收窗口大小(SR协议关键!)
#define SEQ_SIZE 20          // 序列号个数
#define TIMEOUT_THRESHOLD 50

// 数据帧结构
struct DataFrame {
    unsigned char seq;
    char data[1024];
    unsigned char flag; // 0=数据, 1=结束, 2=ACK
};

// 全局变量 - 发送端
BOOL sendAck[SEQ_SIZE];      // 收到的ACK
int sendBase;                // 发送窗口基序号
int nextSeqNum;              // 下一个可用序列号
vector<DataFrame> sendBuffer;// 发送缓冲区

// 全局变量 - 接收端
BOOL recvAck[SEQ_SIZE];      // 已接收的包
map<int, DataFrame> recvBuffer; // 接收缓冲区(乱序包)
int recvBase;                // 接收窗口基序号

void getCurTime(char* ptime) {
    time_t now = time(0);
    strcpy(ptime, ctime(&now));
    ptime[strlen(ptime) - 1] = '\0';
}

// 判断序列号是否在发送窗口内
bool inSendWindow(int seq) {
    if (sendBase <= sendBase + SEND_WIND_SIZE - 1) {
        return seq >= sendBase && seq < sendBase + SEND_WIND_SIZE;
    } else {
        return seq >= sendBase || seq < (sendBase + SEND_WIND_SIZE) % SEQ_SIZE;
    }
}

// 判断序列号是否在接收窗口内
bool inRecvWindow(int seq) {
    if (recvBase <= recvBase + RECV_WIND_SIZE - 1) {
        return seq >= recvBase && seq < recvBase + RECV_WIND_SIZE;
    } else {
        return seq >= recvBase || seq < (recvBase + RECV_WIND_SIZE) % SEQ_SIZE;
    }
}

// 打印发送窗口
void printSendWindow() {
    cout << "[发送窗口] base=" << sendBase << ", next=" << nextSeqNum << endl;
    cout << "  [";
    for (int i = 0; i < SEQ_SIZE; i++) {
        if (i == sendBase) cout << "|";
        if (inSendWindow(i)) {
            cout << (sendAck[i] ? "√" : "×");
        } else {
            cout << " ";
        }
        if (i == (sendBase + SEND_WIND_SIZE - 1) % SEQ_SIZE) cout << "|";
    }
    cout << "]" << endl;
}

// 打印接收窗口
void printRecvWindow() {
    cout << "[接收窗口] base=" << recvBase << endl;
    cout << "  [";
    for (int i = 0; i < SEQ_SIZE; i++) {
        if (i == recvBase) cout << "|";
        if (inRecvWindow(i)) {
            cout << (recvAck[i] ? "√" : "·");
        } else {
            cout << " ";
        }
        if (i == (recvBase + RECV_WIND_SIZE - 1) % SEQ_SIZE) cout << "|";
    }
    cout << "]" << endl;
}

// SR协议:选择重传(只重传未确认的包)
void selectiveRetransmit(SOCKET sockServer, sockaddr_in& clientAddr) {
    cout << "\n[超时] 选择重传未确认的数据包..." << endl;

    int retransmitCount = 0;
    for (int i = sendBase; i != nextSeqNum; i = (i + 1) % SEQ_SIZE) {
        if (!sendAck[i] && i < sendBuffer.size()) {
            sendto(sockServer, (char*)&sendBuffer[i], sizeof(DataFrame), 0,
                  (sockaddr*)&clientAddr, sizeof(clientAddr));
            cout << "[重传] Seq=" << i << ", 数据=\"" << sendBuffer[i].data << "\"" << endl;
            retransmitCount++;
        }
    }

    cout << "[重传] 共重传 " << retransmitCount << " 个数据包" << endl;
    printSendWindow();
}

// 处理收到的ACK(SR协议:独立确认)
void ackHandler(unsigned char ackSeq) {
    cout << "[ACK处理] 收到ACK=" << (int)ackSeq << endl;

    // SR协议:标记该序列号已确认
    if (inSendWindow(ackSeq)) {
        sendAck[ackSeq] = TRUE;
        cout << "[确认] Seq=" << (int)ackSeq << " 已确认" << endl;

        // 如果是窗口基序号,滑动窗口
        while (sendAck[sendBase] && sendBase != nextSeqNum) {
            sendAck[sendBase] = FALSE;
            sendBase = (sendBase + 1) % SEQ_SIZE;
        }

        cout << "[窗口滑动] 新base=" << sendBase << endl;
    } else {
        cout << "[忽略] ACK不在窗口内" << endl;
    }

    printSendWindow();
}

// 处理接收到的数据帧(SR协议:缓存乱序包)
void handleReceivedData(unsigned char seq, const char* data, SOCKET sock, sockaddr_in& clientAddr) {
    cout << "[接收] Seq=" << (int)seq << ", 数据=\"" << data << "\"" << endl;

    if (inRecvWindow(seq)) {
        // 在接收窗口内
        if (!recvAck[seq]) {
            // 新数据包,缓存并标记
            DataFrame frame;
            frame.seq = seq;
            strcpy(frame.data, data);
            recvBuffer[seq] = frame;
            recvAck[seq] = TRUE;

            cout << "[缓存] Seq=" << (int)seq << " 已缓存" << endl;

            // SR协议:立即发送ACK
            DataFrame ackFrame;
            ackFrame.seq = seq;
            ackFrame.flag = 2;
            sendto(sock, (char*)&ackFrame, sizeof(DataFrame), 0,
                  (sockaddr*)&clientAddr, sizeof(clientAddr));
            cout << "[发送ACK] ACK=" << (int)seq << endl;

            // 如果收到recvBase,交付连续的数据
            if (seq == recvBase) {
                cout << "[交付] 开始交付连续数据..." << endl;
                while (recvAck[recvBase]) {
                    if (recvBuffer.find(recvBase) != recvBuffer.end()) {
                        cout << "  [交付] Seq=" << recvBase
                             << ", 数据=\"" << recvBuffer[recvBase].data << "\"" << endl;
                        recvBuffer.erase(recvBase);
                    }
                    recvAck[recvBase] = FALSE;
                    recvBase = (recvBase + 1) % SEQ_SIZE;
                }
                cout << "[窗口滑动] 新recvBase=" << recvBase << endl;
            }
        } else {
            cout << "[重复] Seq=" << (int)seq << " 已收到,重发ACK" << endl;
            // 重发ACK
            DataFrame ackFrame;
            ackFrame.seq = seq;
            ackFrame.flag = 2;
            sendto(sock, (char*)&ackFrame, sizeof(DataFrame), 0,
                  (sockaddr*)&clientAddr, sizeof(clientAddr));
        }

        printRecvWindow();
    } else {
        cout << "[丢弃] Seq=" << (int)seq << " 不在接收窗口内" << endl;
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup失败" << endl;
        return 1;
    }

    SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockServer == INVALID_SOCKET) {
        cout << "创建socket失败" << endl;
        WSACleanup();
        return 1;
    }

    // 非阻塞模式
    u_long iMode = 1;
    ioctlsocket(sockServer, FIONBIO, &iMode);

    sockaddr_in addrServer;
    addrServer.sin_family = AF_INET;
    addrServer.sin_addr.s_addr = INADDR_ANY;
    addrServer.sin_port = htons(SERVER_PORT);

    if (bind(sockServer, (sockaddr*)&addrServer, sizeof(addrServer)) == SOCKET_ERROR) {
        cout << "绑定失败" << endl;
        closesocket(sockServer);
        WSACleanup();
        return 1;
    }

    cout << "========================================" << endl;
    cout << "SR协议服务器(Selective Repeat)" << endl;
    cout << "监听端口: " << SERVER_PORT << endl;
    cout << "发送窗口: " << SEND_WIND_SIZE << endl;
    cout << "接收窗口: " << RECV_WIND_SIZE << " (SR特性!)" << endl;
    cout << "序列号: 0-" << (SEQ_SIZE - 1) << endl;
    cout << "========================================" << endl;

    sockaddr_in addrClient;
    int addrClientLen = sizeof(addrClient);
    char recvBuffer[BUFFER_LENGTH];

    while (true) {
        int recvLen = recvfrom(sockServer, recvBuffer, BUFFER_LENGTH, 0,
                              (sockaddr*)&addrClient, &addrClientLen);

        if (recvLen > 0) {
            recvBuffer[recvLen] = '\0';

            if (recvLen == sizeof(DataFrame)) {
                DataFrame* frame = (DataFrame*)recvBuffer;

                if (frame->flag == 2) {
                    // ACK帧
                    ackHandler(frame->seq);
                } else {
                    // 数据帧
                    handleReceivedData(frame->seq, frame->data, sockServer, addrClient);
                }
            } else {
                // 命令
                string command(recvBuffer);
                cout << "\n[命令] " << command << endl;

                if (command == "-time") {
                    char timeStr[100];
                    getCurTime(timeStr);
                    sendto(sockServer, timeStr, strlen(timeStr), 0,
                          (sockaddr*)&addrClient, addrClientLen);
                }
                else if (command == "-quit") {
                    string goodbye = "Good bye!";
                    sendto(sockServer, goodbye.c_str(), goodbye.length(), 0,
                          (sockaddr*)&addrClient, addrClientLen);
                }
                else if (command == "-testsr") {
                    cout << "\n[开始] SR协议测试" << endl;
                    cout << "========================================" << endl;

                    // 初始化
                    for (int i = 0; i < SEQ_SIZE; i++) {
                        sendAck[i] = FALSE;
                        recvAck[i] = FALSE;
                    }
                    sendBase = 0;
                    nextSeqNum = 0;
                    recvBase = 0;
                    sendBuffer.clear();
                    recvBuffer.clear();

                    // 准备数据
                    const char* testData[] = {
                        "SR-1:选择重传协议",
                        "SR-2:接收窗口>1",
                        "SR-3:缓存乱序包",
                        "SR-4:选择重传",
                        "SR-5:独立ACK",
                        "SR-6:效率最高",
                        "SR-7:滑动窗口",
                        "SR-8:可靠传输",
                        "SR-9:网络协议",
                        "SR-10:计算机网络",
                        "SR-11:传输层",
                        "SR-12:UDP封装",
                        "SR-13:丢包恢复",
                        "SR-14:乱序处理",
                        "SR-15:测试完成"
                    };
                    int totalPacket = 15;

                    // 准备缓冲区
                    for (int i = 0; i < totalPacket; i++) {
                        DataFrame frame;
                        frame.seq = i % SEQ_SIZE;
                        strcpy(frame.data, testData[i]);
                        frame.flag = (i == totalPacket - 1) ? 1 : 0;
                        sendBuffer.push_back(frame);
                    }

                    printSendWindow();

                    // SR发送循环
                    int timer = 0;
                    bool transferComplete = false;

                    while (!transferComplete) {
                        // 在窗口允许下发送
                        while (inSendWindow(nextSeqNum) && nextSeqNum < totalPacket) {
                            sendto(sockServer, (char*)&sendBuffer[nextSeqNum], sizeof(DataFrame), 0,
                                  (sockaddr*)&addrClient, addrClientLen);

                            cout << "[发送] Seq=" << nextSeqNum
                                 << ", 数据=\"" << sendBuffer[nextSeqNum].data << "\"" << endl;

                            nextSeqNum++;
                            timer = 0;
                        }

                        // 接收ACK
                        DataFrame ackFrame;
                        sockaddr_in fromAddr;
                        int fromLen = sizeof(fromAddr);

                        int ackLen = recvfrom(sockServer, (char*)&ackFrame, sizeof(DataFrame), 0,
                                             (sockaddr*)&fromAddr, &fromLen);

                        if (ackLen > 0 && ackFrame.flag == 2) {
                            ackHandler(ackFrame.seq);
                            timer = 0;

                            // 检查完成
                            if (sendBase == totalPacket % SEQ_SIZE && nextSeqNum == totalPacket) {
                                transferComplete = true;
                            }
                        } else {
                            timer++;
                            if (timer >= TIMEOUT_THRESHOLD) {
                                selectiveRetransmit(sockServer, addrClient);
                                timer = 0;
                            }
                        }

                        Sleep(1);
                    }

                    cout << "\n========================================" << endl;
                    cout << "[完成] SR协议测试完成!" << endl;
                    cout << "发送: " << totalPacket << " 个数据包" << endl;
                    cout << "========================================" << endl;
                }
            }
        }

        Sleep(1);
    }

    closesocket(sockServer);
    WSACleanup();

    return 0;
}
