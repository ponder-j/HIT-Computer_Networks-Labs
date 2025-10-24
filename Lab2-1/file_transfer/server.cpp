#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <direct.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define SERVER_PORT 12340
#define DATA_SIZE 1020  // 每个数据帧的数据大小
#define TIMEOUT_MS 2000

// 数据帧结构
struct DataFrame {
    unsigned char seq;          // 序列号 (0或1)
    unsigned int fileSize;      // 文件总大小(仅在第一帧)
    unsigned int offset;        // 当前数据在文件中的偏移
    unsigned short dataLen;     // 当前帧的数据长度
    char data[DATA_SIZE];       // 数据
    unsigned char flag;         // 0=数据帧, 1=最后一帧, 2=ACK, 3=文件列表, 4=文件信息帧
};

// 文件信息结构
struct FileInfo {
    char fileName[256];
    unsigned int fileSize;
};

// 全局变量
float lossRate = 0.2f;  // ACK丢包率（可修改此值，范围0.0-1.0）

// 模拟丢包
bool simulateLoss() {
    if (lossRate <= 0.0f) return false;
    return ((float)rand() / RAND_MAX) < lossRate;
}

// 获取当前时间
string getCurrentTime() {
    time_t now = time(0);
    char* dt = ctime(&now);
    string timeStr(dt);
    timeStr.pop_back();
    return timeStr;
}

// 发送ACK，返回是否成功发送
bool sendAck(SOCKET sock, sockaddr_in& clientAddr, unsigned char ackSeq) {
    // 模拟ACK丢失
    if (simulateLoss()) {
        cout << "[模拟] ACK丢失!" << endl;
        return false;
    }

    DataFrame ackFrame;
    ackFrame.seq = ackSeq;
    ackFrame.flag = 2;
    ackFrame.dataLen = 0;

    sendto(sock, (char*)&ackFrame, sizeof(DataFrame), 0,
           (sockaddr*)&clientAddr, sizeof(clientAddr));

    return true;  // ACK成功发送
}

// 接收文件
bool receiveFile(SOCKET sock, sockaddr_in& clientAddr, const char* savePath) {
    unsigned char expectedSeq = 0;
    unsigned int totalSize = 0;
    unsigned int receivedSize = 0;
    int packetCount = 0;

    // 打开文件用于写入
    ofstream outFile(savePath, ios::binary);
    if (!outFile.is_open()) {
        cout << "[错误] 无法创建文件: " << savePath << endl;
        return false;
    }

    cout << "[接收] 开始接收文件..." << endl;
    cout << "保存路径: " << savePath << endl;

    // 设置超时
    DWORD timeout = 30000; // 30秒
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    while (true) {
        DataFrame frame;
        sockaddr_in fromAddr;
        int fromLen = sizeof(fromAddr);

        int recvLen = recvfrom(sock, (char*)&frame, sizeof(DataFrame), 0,
                              (sockaddr*)&fromAddr, &fromLen);

        if (recvLen > 0 && frame.flag != 2) {
            // 如果收到重复的文件信息帧(flag==4)，重发ACK确认但不写入数据、不改变期望序列号
            if (frame.flag == 4) {
                // 若之前未记录文件大小，可更新一下显示用途
                if (totalSize == 0 && frame.fileSize > 0) {
                    totalSize = frame.fileSize;
                    cout << "[信息] 文件大小: " << totalSize << " 字节" << endl;
                }
                cout << "[提示] 收到重复的文件信息帧，重发ACK确认" << endl;
                sendAck(sock, clientAddr, 0);
                continue;
            }

            packetCount++;

            // 第一帧数据可能携带文件大小信息
            if (totalSize == 0 && frame.fileSize > 0) {
                totalSize = frame.fileSize;
                cout << "[信息] 文件大小: " << totalSize << " 字节" << endl;
            }

            cout << "[接收] 包#" << packetCount << ", Seq=" << (int)frame.seq
                 << ", 偏移=" << frame.offset
                 << ", 数据=" << frame.dataLen << "字节" << endl;

            // 检查序列号
            if (frame.seq == expectedSeq) {
                // 发送ACK，只有成功发送才写入数据和切换序列号
                if (sendAck(sock, clientAddr, expectedSeq)) {
                    cout << "[ACK] 发送ACK=" << (int)expectedSeq << endl;

                    // 写入数据（此处为顺序写，若需要也可根据offset定位）
                    outFile.write(frame.data, frame.dataLen);
                    receivedSize += frame.dataLen;

                    // 切换期望序列号
                    expectedSeq = 1 - expectedSeq;

                    // 显示进度
                    if (totalSize > 0) {
                        float progress = (float)receivedSize / totalSize * 100;
                        cout << "[进度] " << receivedSize << "/" << totalSize
                             << " (" << progress << "%)" << endl;
                    }

                    // 检查是否为最后一帧
                    if (frame.flag == 1) {
                        cout << "\n[完成] 文件接收完成!" << endl;
                        cout << "总大小: " << receivedSize << " 字节" << endl;
                        cout << "总包数: " << packetCount << endl;
                        outFile.close();
                        return true;
                    }
                }
                // 如果ACK发送失败（目前总是成功，但预留了扩展空间）
            } else {
                cout << "[错误] 序列号不匹配，期望=" << (int)expectedSeq
                     << ", 实际=" << (int)frame.seq << endl;
                // 重发上一个ACK
                sendAck(sock, clientAddr, 1 - expectedSeq);
            }
        } else if (recvLen == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                cout << "[超时] 接收文件超时" << endl;
                outFile.close();
                return false;
            }
        }
    }

    outFile.close();
    return false;
}

int main() {
    // 初始化随机数生成器
    srand((unsigned int)time(NULL));

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup失败" << endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        cout << "创建socket失败" << endl;
        WSACleanup();
        return 1;
    }

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
    cout << "文件传输服务器(基于停等协议)" << endl;
    cout << "监听端口: " << SERVER_PORT << endl;
    cout << "数据帧大小: " << DATA_SIZE << " 字节" << endl;
    cout << "ACK丢包率: " << (lossRate * 100) << "%" << endl;
    cout << "========================================" << endl;

    // 创建接收目录
    _mkdir("received_files");

    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    while (true) {
        char recvBuffer[sizeof(DataFrame)];

        cout << "\n等待客户端请求..." << endl;

        int recvLen = recvfrom(serverSocket, recvBuffer, sizeof(DataFrame), 0,
                              (sockaddr*)&clientAddr, &clientAddrLen);

        if (recvLen > 0) {
            DataFrame* request = (DataFrame*)recvBuffer;

            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);

            if (request->flag == 4) {
                // 文件上传请求
                cout << "\n[请求] 来自 " << clientIP << " - 上传文件" << endl;
                cout << "文件名: " << request->data << endl;
                cout << "文件大小: " << request->fileSize << " 字节" << endl;

                // 发送ACK确认可以开始传输
                sendAck(serverSocket, clientAddr, 0);

                // 接收文件
                string savePath = "received_files\\";
                savePath += request->data;

                if (receiveFile(serverSocket, clientAddr, savePath.c_str())) {
                    cout << "[成功] 文件保存到: " << savePath << endl;
                } else {
                    cout << "[失败] 文件接收失败" << endl;
                }
            }
            else if (request->flag == 2) {
                // ACK帧，忽略
                continue;
            }
            else {
                // 其他命令
                request->data[request->dataLen] = '\0';
                string command(request->data);

                cout << "[请求] 来自 " << clientIP << " - " << command << endl;

                if (command == "-time") {
                    string timeStr = getCurrentTime();
                    DataFrame response;
                    response.seq = 0;
                    response.flag = 0;
                    response.dataLen = timeStr.length();
                    strcpy_s(response.data, timeStr.c_str());

                    sendto(serverSocket, (char*)&response, sizeof(DataFrame), 0,
                          (sockaddr*)&clientAddr, clientAddrLen);
                }
                else if (command == "-quit") {
                    DataFrame response;
                    response.seq = 0;
                    response.flag = 0;
                    string goodbye = "Good bye!";
                    response.dataLen = goodbye.length();
                    strcpy_s(response.data, goodbye.c_str());

                    sendto(serverSocket, (char*)&response, sizeof(DataFrame), 0,
                          (sockaddr*)&clientAddr, clientAddrLen);

                    cout << "[断开] 客户端断开连接" << endl;
                }
            }
        }
    }

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
