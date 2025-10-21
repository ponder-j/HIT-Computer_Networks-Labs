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

// 服务器配置
#define SERVER_PORT 12340
#define BUFFER_LENGTH 1026
#define SEND_WIND_SIZE 10    // 发送窗口大小
#define SEQ_SIZE 20          // 序列号空间大小(需要 >= 2*窗口大小)
#define TIMEOUT_THRESHOLD 100 // 超时阈值(ms)
#define MAX_RETRIES 10       // 最大重传次数

// 数据帧结构
struct DataFrame {
    unsigned char seq;       // 序列号
    char data[1024];         // 数据
    unsigned char flag;      // 标志位: 0=普通帧, 1=结束帧
};

// ACK帧结构
struct AckFrame {
    unsigned char ack;       // ACK序列号
    unsigned char flag;      // 预留标志位
};

// 发送方窗口状态
struct SendWindow {
    bool acked;              // 是否已确认
    clock_t sendTime;        // 发送时间(用于超时判断)
    int retryCount;          // 重传次数
};

// 全局变量
map<int, SendWindow> sendWindow;  // 发送窗口(包索引 -> 状态)
int sendBase;                     // 发送窗口基包索引(0-totalPacket-1)
int nextSeqNum;                   // 下一个待发送的包索引
int totalPacket;                  // 需要发送的包总数
vector<DataFrame> packets;        // 数据包缓冲区

// 获取当前时间
void getCurTime(char* ptime) {
    time_t now = time(0);
    strcpy(ptime, ctime(&now));
    ptime[strlen(ptime) - 1] = '\0';
}

// 判断序列号是否在窗口内
bool inWindow(int seq, int base, int windowSize) {
    int dist = (seq + SEQ_SIZE - base) % SEQ_SIZE;
    return dist < windowSize;
}

// 打印发送窗口状态
void printWindow() {
    cout << "[窗口状态] sendBase(包索引)=" << sendBase;
    if (sendBase < totalPacket) {
        cout << ", Seq=" << (int)packets[sendBase].seq;
    } else {
        cout << " (已完成)";
    }
    cout << ", nextSeqNum(包索引)=" << nextSeqNum << endl;
    
    if (sendBase < totalPacket) {
        cout << "  窗口范围(包索引): [" << sendBase << ", " << (sendBase + SEND_WIND_SIZE - 1) << "]" << endl;
        cout << "  [";
        for (int i = 0; i < SEND_WIND_SIZE; i++) {
            int pktIndex = sendBase + i;
            if (pktIndex >= totalPacket) {
                cout << " ";
            } else if (sendWindow.find(pktIndex) != sendWindow.end()) {
                cout << (sendWindow[pktIndex].acked ? "√" : "×");
            } else {
                cout << " ";
            }
        }
        cout << "]" << endl;
    }
}

// 发送单个数据包
void sendPacket(SOCKET sockServer, sockaddr_in& clientAddr, int pktIndex) {
    if (pktIndex < totalPacket) {
        sendto(sockServer, (char*)&packets[pktIndex], sizeof(DataFrame), 0,
              (sockaddr*)&clientAddr, sizeof(clientAddr));
        
        // 更新发送窗口状态（使用包索引作为key）
        sendWindow[pktIndex].sendTime = clock();
        sendWindow[pktIndex].acked = false;
        
        cout << "[发送] 包索引=" << pktIndex 
             << ", Seq=" << (int)packets[pktIndex].seq
             << ", 数据=\"" << packets[pktIndex].data << "\"" << endl;
    }
}

// 检查超时并重传
void checkTimeout(SOCKET sockServer, sockaddr_in& clientAddr) {
    clock_t now = clock();
    
    for (int i = 0; i < SEND_WIND_SIZE; i++) {
        int pktIndex = sendBase + i;
        
        if (pktIndex >= totalPacket) break;
        
        // 检查是否已发送但未确认
        if (sendWindow.find(pktIndex) != sendWindow.end() && !sendWindow[pktIndex].acked) {
            double elapsed = (double)(now - sendWindow[pktIndex].sendTime) * 1000.0 / CLOCKS_PER_SEC;
            
            if (elapsed >= TIMEOUT_THRESHOLD) {
                sendWindow[pktIndex].retryCount++;
                
                if (sendWindow[pktIndex].retryCount >= MAX_RETRIES) {
                    // 达到最大重传次数，只打印一次警告
                    if (sendWindow[pktIndex].retryCount == MAX_RETRIES) {
                        cout << "[警告] 包索引=" << pktIndex 
                             << ", Seq=" << (int)packets[pktIndex].seq
                             << " 达到最大重传次数" << endl;
                    }
                    continue;
                }
                
                cout << "[超时重传] 包索引=" << pktIndex 
                     << ", Seq=" << (int)packets[pktIndex].seq
                     << " (第" << sendWindow[pktIndex].retryCount << "次)" << endl;
                sendPacket(sockServer, clientAddr, pktIndex);
            }
        }
    }
}

// 处理收到的ACK
void ackHandler(unsigned char ackSeq) {
    cout << "[ACK处理] 收到ACK=" << (int)ackSeq << endl;
    
    // 查找对应的包索引
    int pktIndex = -1;
    for (int i = 0; i < totalPacket; i++) {
        if (packets[i].seq == ackSeq) {
            pktIndex = i;
            break;
        }
    }
    
    if (pktIndex == -1) {
        cout << "[警告] 收到未知的ACK=" << (int)ackSeq << endl;
        return;
    }
    
    // 标记该包已确认
    if (sendWindow.find(pktIndex) != sendWindow.end()) {
        sendWindow[pktIndex].acked = true;
        cout << "[确认] 包索引=" << pktIndex << ", Seq=" << (int)ackSeq << " 已确认" << endl;
    } else {
        cout << "[忽略] 包索引=" << pktIndex << " 不在发送窗口内" << endl;
        return;
    }
    
    // 滑动窗口：移除已确认的连续包
    while (sendBase < totalPacket && 
           sendWindow.find(sendBase) != sendWindow.end() && 
           sendWindow[sendBase].acked) {
        sendWindow.erase(sendBase);
        int oldBase = sendBase;
        sendBase++;
        cout << "[窗口滑动] 新的sendBase=" << sendBase;
        if (sendBase < totalPacket) {
            cout << ", Seq=" << (int)packets[sendBase].seq << endl;
        } else {
            cout << " (已完成)" << endl;
        }
    }
    
    printWindow();
}

int main() {
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup失败" << endl;
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
        cout << "绑定失败" << endl;
        closesocket(sockServer);
        WSACleanup();
        return 1;
    }

    cout << "======================================" << endl;
    cout << "SR协议服务器已启动" << endl;
    cout << "监听端口: " << SERVER_PORT << endl;
    cout << "发送窗口大小: " << SEND_WIND_SIZE << endl;
    cout << "序列号空间: 0-" << (SEQ_SIZE - 1) << endl;
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
            else if (command == "-testsr") {
                cout << "\n[开始] SR协议测试" << endl;
                cout << "======================================" << endl;

                // 初始化SR参数
                sendWindow.clear();
                sendBase = 0;
                nextSeqNum = 0;
                packets.clear();

                // 准备测试数据
                const char* testData[] = {
                    "数据包1:SR协议测试开始",
                    "数据包2:Selective-Repeat测试",
                    "数据包3:选择重传协议",
                    "数据包4:接收缓冲区支持",
                    "数据包5:乱序接收",
                    "数据包6:独立ACK机制",
                    "数据包7:选择性重传",
                    "数据包8:提高传输效率",
                    "数据包9:窗口大小10",
                    "数据包10:序列号空间20",
                    "数据包11:超时独立计时",
                    "数据包12:丢包重传优化",
                    "数据包13:网络协议实验",
                    "数据包14:计算机网络",
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

                // SR发送循环
                bool transferComplete = false;
                int idleCount = 0;
                int stableMaxRetryCount = 0;  // 稳定的最大重传计数

                while (!transferComplete) {
                    // 发送窗口内的新数据包
                    while (nextSeqNum < sendBase + SEND_WIND_SIZE && nextSeqNum < totalPacket) {
                        sendPacket(sockServer, addrClient, nextSeqNum);
                        nextSeqNum++;
                        idleCount = 0;
                        stableMaxRetryCount = 0;
                    }

                    // 检查超时并重传
                    checkTimeout(sockServer, addrClient);
                    
                    // 检查是否所有未确认的包都达到最大重传次数
                    if (nextSeqNum >= totalPacket && sendBase < totalPacket) {
                        int unackedCount = 0;
                        int maxRetryReached = 0;
                        
                        for (int i = sendBase; i < totalPacket; i++) {
                            if (sendWindow.find(i) != sendWindow.end() && !sendWindow[i].acked) {
                                unackedCount++;
                                if (sendWindow[i].retryCount >= MAX_RETRIES) {
                                    maxRetryReached++;
                                }
                            }
                        }
                        
                        if (unackedCount > 0 && maxRetryReached == unackedCount) {
                            stableMaxRetryCount++;
                            // 连续检测到20次（约20ms），确认所有包都无法送达
                            if (stableMaxRetryCount >= 20) {
                                cout << "\n[错误] 所有剩余包均达到最大重传次数，传输失败" << endl;
                                cout << "未确认包数: " << unackedCount << endl;
                                transferComplete = true;
                            }
                        } else {
                            stableMaxRetryCount = 0;
                        }
                    }

                    // 尝试接收ACK
                    AckFrame ackFrame;
                    sockaddr_in fromAddr;
                    int fromLen = sizeof(fromAddr);

                    int ackLen = recvfrom(sockServer, (char*)&ackFrame, sizeof(AckFrame), 0,
                                         (sockaddr*)&fromAddr, &fromLen);

                    if (ackLen > 0) {
                        ackHandler(ackFrame.ack);
                        idleCount = 0;
                        stableMaxRetryCount = 0;
                        
                        // 检查是否所有数据都已确认
                        if (sendBase >= totalPacket) {
                            transferComplete = true;
                        }
                    } else {
                        idleCount++;
                        
                        // 备用退出条件：长时间无响应（约10秒）
                        if (nextSeqNum >= totalPacket && idleCount > 10000) {
                            int unackedCount = 0;
                            for (int i = sendBase; i < totalPacket; i++) {
                                if (sendWindow.find(i) != sendWindow.end() && !sendWindow[i].acked) {
                                    unackedCount++;
                                }
                            }
                            
                            if (unackedCount == 0) {
                                cout << "\n[完成] 所有包已确认" << endl;
                            } else {
                                cout << "\n[超时] 长时间无响应，传输终止" << endl;
                                cout << "[未完成] 还有 " << unackedCount << " 个包未确认" << endl;
                            }
                            transferComplete = true;
                        }
                        
                        Sleep(1);
                    }
                }

                cout << "\n======================================" << endl;
                cout << "[完成] SR协议测试完成!" << endl;
                cout << "总共发送: " << totalPacket << " 个数据包" << endl;
                cout << "最终sendBase: " << sendBase << endl;
                cout << "======================================" << endl;
            }
            else {
                // 回显
                sendto(sockServer, recvBuffer, recvLen, 0,
                      (sockaddr*)&addrClient, addrClientLen);
                cout << "[响应] 回显数据" << endl;
            }
        }

        Sleep(1);
    }

    closesocket(sockServer);
    WSACleanup();
    return 0;
}
