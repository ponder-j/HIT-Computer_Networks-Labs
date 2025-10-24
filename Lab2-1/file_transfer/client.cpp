#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <io.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define SERVER_PORT 12340
#define SERVER_IP "127.0.0.1"
#define DATA_SIZE 1020
#define TIMEOUT_MS 2000

// 数据帧结构
struct DataFrame {
    unsigned char seq;
    unsigned int fileSize;
    unsigned int offset;
    unsigned short dataLen;
    char data[DATA_SIZE];
    unsigned char flag; // 0=数据, 1=结束, 2=ACK, 3=文件列表, 4=文件信息
};

// 发送文件
bool sendFile(SOCKET sock, sockaddr_in& serverAddr, const char* filePath) {
    // 打开文件
    ifstream inFile(filePath, ios::binary | ios::ate);
    if (!inFile.is_open()) {
        cout << "[错误] 无法打开文件: " << filePath << endl;
        return false;
    }

    // 获取文件大小
    unsigned int fileSize = (unsigned int)inFile.tellg();
    inFile.seekg(0, ios::beg);

    // 获取文件名
    string fullPath(filePath);
    size_t pos = fullPath.find_last_of("\\/");
    string fileName = (pos != string::npos) ? fullPath.substr(pos + 1) : fullPath;

    cout << "\n[上传] 文件: " << fileName << endl;
    cout << "大小: " << fileSize << " 字节" << endl;
    cout << "========================================" << endl;

    // 为后续select/recvfrom准备复用的变量
    fd_set readSet;
    timeval timeout;
    sockaddr_in fromAddr;
    int fromLen = 0;

    // 发送文件信息帧
    DataFrame infoFrame;
    infoFrame.seq = 0;
    infoFrame.flag = 4;
    infoFrame.fileSize = fileSize;
    infoFrame.dataLen = fileName.length();
    strcpy_s(infoFrame.data, fileName.c_str());

    // 发送文件信息帧并等待ACK，带重传机制
    int attempts = 0;
    const int MAX_ATTEMPTS = 5;
    bool ackReceived = false;
    cout << "[发送] 文件信息帧..." << endl;

    while (attempts < MAX_ATTEMPTS && !ackReceived) {
        // 发送
        sendto(sock, (char*)&infoFrame, sizeof(DataFrame), 0,
               (sockaddr*)&serverAddr, sizeof(serverAddr));
        cout << "[尝试] 第 " << (attempts + 1) << " 次发送文件信息帧" << endl;

        // 等待服务器ACK确认
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);

        timeval timeout;
        timeout.tv_sec = 2; // 将超时缩短为2秒，以便更快重传
        timeout.tv_usec = 0;

        if (select(0, &readSet, NULL, NULL, &timeout) > 0) {
            DataFrame ackFrame;
            fromLen = sizeof(fromAddr);
            recvfrom(sock, (char*)&ackFrame, sizeof(DataFrame), 0,
                     (sockaddr*)&fromAddr, &fromLen);
            
            // 假设服务器用一个特定的ACK进行文件传输的初始确认
            if (ackFrame.flag == 2 && ackFrame.seq == 0) {
                 ackReceived = true;
            }
        } else {
            cout << "[超时] 未收到文件信息帧的ACK，准备重传..." << endl;
        }
        attempts++;
    }

    if (!ackReceived) {
        cout << "[错误] 服务器无响应，文件信息发送失败" << endl;
        inFile.close();
        return false;
    }

    cout << "[确认] 服务器准备接收" << endl;

    // 发送文件数据
    unsigned char currentSeq = 0;
    unsigned int offset = 0;
    int packetCount = 0;
    char buffer[DATA_SIZE];

    while (!inFile.eof()) {
        // 读取数据
        inFile.read(buffer, DATA_SIZE);
        streamsize bytesRead = inFile.gcount();

        if (bytesRead == 0) break;

        packetCount++;
        bool isLastPacket = inFile.eof();

        // 构造数据帧
        DataFrame frame;
        frame.seq = currentSeq;
        frame.fileSize = (packetCount == 1) ? fileSize : 0;
        frame.offset = offset;
        frame.dataLen = (unsigned short)bytesRead;
        memcpy(frame.data, buffer, bytesRead);
        frame.flag = isLastPacket ? 1 : 0;

        // 发送数据帧(带重传机制)
        int attempts = 0;
        const int MAX_ATTEMPTS = 5;
        bool success = false;

        while (attempts < MAX_ATTEMPTS && !success) {
            // 发送
            sendto(sock, (char*)&frame, sizeof(DataFrame), 0,
                   (sockaddr*)&serverAddr, sizeof(serverAddr));

            cout << "[发送] 包#" << packetCount << ", Seq=" << (int)currentSeq
                 << ", 偏移=" << offset << ", 大小=" << bytesRead
                 << " 字节, 尝试=" << (attempts + 1) << endl;

            // 等待ACK
            FD_ZERO(&readSet);
            FD_SET(sock, &readSet);

            timeout.tv_sec = TIMEOUT_MS / 1000;
            timeout.tv_usec = (TIMEOUT_MS % 1000) * 1000;

            int selectResult = select(0, &readSet, NULL, NULL, &timeout);

            if (selectResult > 0) {
                DataFrame ack;
                fromLen = sizeof(fromAddr);
                recvfrom(sock, (char*)&ack, sizeof(DataFrame), 0,
                        (sockaddr*)&fromAddr, &fromLen);

                if (ack.flag == 2) { // ACK帧
                    cout << "[ACK] 收到ACK=" << (int)ack.seq << endl;

                    if (ack.seq == currentSeq) {
                        cout << "[成功] ACK正确" << endl;
                        success = true;

                        // 更新进度
                        float progress = (float)(offset + bytesRead) / fileSize * 100;
                        cout << "[进度] " << (offset + bytesRead) << "/" << fileSize
                             << " (" << progress << "%)" << endl;
                    } else {
                        cout << "[警告] ACK序列号不匹配" << endl;
                    }
                }
            } else if (selectResult == 0) {
                cout << "[超时] 准备重传..." << endl;
            }

            attempts++;
        }

        if (!success) {
            cout << "[失败] 数据包发送失败" << endl;
            inFile.close();
            return false;
        }

        // 更新状态
        currentSeq = 1 - currentSeq;
        offset += bytesRead;
    }

    inFile.close();

    cout << "\n========================================" << endl;
    cout << "[完成] 文件上传成功!" << endl;
    cout << "总大小: " << fileSize << " 字节" << endl;
    cout << "总包数: " << packetCount << endl;
    cout << "========================================" << endl;

    return true;
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

    cout << "========================================" << endl;
    cout << "文件传输客户端(基于停等协议)" << endl;
    cout << "服务器: " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "========================================" << endl;
    cout << "\n命令:" << endl;
    cout << "  -time               获取服务器时间" << endl;
    cout << "  -upload <文件路径>   上传文件" << endl;
    cout << "      示例: -upload test.txt" << endl;
    cout << "  -quit               退出" << endl;
    cout << "\n说明:" << endl;
    cout << "  - ACK丢包率由服务器端配置控制" << endl;
    cout << "========================================\n" << endl;

    while (true) {
        cout << "\n请输入命令: ";
        string input;
        getline(cin, input);

        if (input.empty()) continue;

        if (input == "-time") {
            DataFrame request;
            request.seq = 0;
            request.flag = 0;
            string cmd = "-time";
            request.dataLen = cmd.length();
            strcpy_s(request.data, cmd.c_str());

            sendto(clientSocket, (char*)&request, sizeof(DataFrame), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            DataFrame response;
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvLen = recvfrom(clientSocket, (char*)&response, sizeof(DataFrame), 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                response.data[response.dataLen] = '\0';
                cout << "[服务器时间] " << response.data << endl;
            }
        }
        else if (input == "-quit") {
            DataFrame request;
            request.seq = 0;
            request.flag = 0;
            string cmd = "-quit";
            request.dataLen = cmd.length();
            strcpy_s(request.data, cmd.c_str());

            sendto(clientSocket, (char*)&request, sizeof(DataFrame), 0,
                  (sockaddr*)&serverAddr, sizeof(serverAddr));

            DataFrame response;
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvLen = recvfrom(clientSocket, (char*)&response, sizeof(DataFrame), 0,
                                  (sockaddr*)&fromAddr, &fromLen);

            if (recvLen > 0) {
                response.data[response.dataLen] = '\0';
                cout << "[服务器] " << response.data << endl;
            }

            break;
        }
        else if (input.substr(0, 7) == "-upload") {
            // 解析命令
            size_t pos = 8;
            if (input.length() <= pos) {
                cout << "[错误] 请指定文件路径" << endl;
                cout << "用法: -upload <文件路径>" << endl;
                continue;
            }

            // 提取文件路径
            string filePath = input.substr(pos);

            // 检查文件是否存在
            if (_access(filePath.c_str(), 0) != 0) {
                cout << "[错误] 文件不存在: " << filePath << endl;
                continue;
            }

            // 上传文件
            sendFile(clientSocket, serverAddr, filePath.c_str());
        }
        else {
            cout << "[错误] 未知命令: " << input << endl;
            cout << "输入命令查看帮助" << endl;
        }
    }

    closesocket(clientSocket);
    WSACleanup();

    cout << "\n客户端已关闭" << endl;

    return 0;
}
