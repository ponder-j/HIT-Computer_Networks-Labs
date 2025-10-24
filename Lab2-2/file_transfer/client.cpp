#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <io.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define SERVER_PORT 12340
#define SERVER_IP "127.0.0.1"
#define DATA_SIZE 1020
#define SEQ_SIZE 32             // 序列号空间大小(必须 >= 2*WINDOW_SIZE)
#define WINDOW_SIZE 10          // 发送窗口大小
#define TIMEOUT_MS 500          // 超时重传间隔(ms)
#define MAX_RETRY 50            // 单分片最大重传次数
#define INFO_SEQ 255            // 文件信息帧专用序列号，避免与数据ACK冲突

struct DataFrame {
    unsigned char seq;          // 序列号 0..SEQ_SIZE-1
    unsigned int fileSize;      // 文件总大小（信息帧使用）
    unsigned int offset;        // 数据偏移
    unsigned short dataLen;     // 数据长度
    char data[DATA_SIZE];       // 数据
    unsigned char flag;         // 0=数据, 1=最后, 2=ACK(仅接收用), 4=信息
};

struct SegmentState {
    DataFrame frame;            // 分片数据
    bool sent = false;          // 是否已发送
    bool acked = false;         // 是否已确认
    clock_t lastSend = 0;       // 上次发送时间
    int retries = 0;            // 已重传次数
};

static inline clock_t now() { return clock(); }

// 从socket接收ACK（非阻塞，带超时）
bool recvAck(SOCKET sock, unsigned char &ackSeq, int waitMs) {
    fd_set readSet; FD_ZERO(&readSet); FD_SET(sock, &readSet);
    timeval tv{}; tv.tv_sec = waitMs / 1000; tv.tv_usec = (waitMs % 1000) * 1000;
    int r = select(0, &readSet, NULL, NULL, &tv);
    if (r > 0) {
        DataFrame ack{}; sockaddr_in from{}; int fromLen = sizeof(from);
        int len = recvfrom(sock, (char*)&ack, sizeof(ack), 0, (sockaddr*)&from, &fromLen);
        if (len > 0 && ack.flag == 2) { ackSeq = ack.seq; return true; }
    }
    return false;
}

// 可靠发送信息帧（握手）
bool sendFileInfo(SOCKET sock, sockaddr_in &serverAddr, const string &fileName, unsigned int fileSize) {
    DataFrame info{};
    info.seq = INFO_SEQ; // 使用专用序列号，避免与数据分片冲突
    info.flag = 4;
    info.fileSize = fileSize;
    info.dataLen = (unsigned short)min<size_t>(fileName.size(), DATA_SIZE);
    memcpy(info.data, fileName.c_str(), info.dataLen);

    cout << "[发送] 文件信息帧: name=" << fileName << ", size=" << fileSize << endl;

    for (int attempt = 1; attempt <= 20; ++attempt) {
        sendto(sock, (char*)&info, sizeof(info), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
        cout << "  尝试#" << attempt << " 等待ACK..." << endl;
        unsigned char ackSeq = 255;
        if (recvAck(sock, ackSeq, 500)) {
            if (ackSeq == INFO_SEQ) {
                cout << "  [确认] 收到信息帧ACK" << endl;
                // 清理可能滞留在缓冲区里的多余ACK
                while (true) {
                    fd_set rs; FD_ZERO(&rs); FD_SET(sock, &rs);
                    timeval tv0{}; tv0.tv_sec = 0; tv0.tv_usec = 0;
                    int rr = select(0, &rs, NULL, NULL, &tv0);
                    if (rr <= 0) break;
                    DataFrame dump{}; sockaddr_in f{}; int fl = sizeof(f);
                    int l = recvfrom(sock, (char*)&dump, sizeof(dump), 0, (sockaddr*)&f, &fl);
                    (void)l;
                }
                return true;
            }
        }
    }
    cout << "[错误] 服务器未响应信息帧" << endl;
    return false;
}

int main() {
    srand((unsigned int)time(NULL));

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { cout << "WSAStartup失败" << endl; return 1; }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) { cout << "创建socket失败" << endl; WSACleanup(); return 1; }

    sockaddr_in serverAddr{}; serverAddr.sin_family = AF_INET; serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    cout << "========================================" << endl;
    cout << "SR 文件传输客户端" << endl;
    cout << "服务器: " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "窗口大小: " << WINDOW_SIZE << ", 序列空间: " << SEQ_SIZE << endl;
    cout << "========================================\n" << endl;

    while (true) {
        cout << "请输入命令 (-upload <文件路径> | -quit): ";
        string line; getline(cin, line); if (line.empty()) continue;
        if (line == "-quit") break;

        if (line.rfind("-upload", 0) == 0) {
            // 解析文件路径
            size_t pos = line.find(' ');
            if (pos == string::npos || pos + 1 >= line.size()) { cout << "[错误] 用法: -upload <文件路径>" << endl; continue; }
            string path = line.substr(pos + 1);

            if (_access(path.c_str(), 0) != 0) { cout << "[错误] 文件不存在: " << path << endl; continue; }

            // 打开文件
            ifstream in(path, ios::binary | ios::ate);
            if (!in.is_open()) { cout << "[错误] 无法打开文件" << endl; continue; }
            unsigned int fileSize = (unsigned int)in.tellg(); in.seekg(0, ios::beg);

            // 文件名
            string fileName = path;
            size_t p = fileName.find_last_of("\\/");
            if (p != string::npos) fileName = fileName.substr(p + 1);

            // 握手：发送信息帧
            if (!sendFileInfo(sock, serverAddr, fileName, fileSize)) { in.close(); continue; }

            // 拆分分片
            vector<SegmentState> segs; segs.reserve((fileSize + DATA_SIZE - 1) / DATA_SIZE);
            unsigned int offset = 0; int idx = 0;
            while (offset < fileSize) {
                DataFrame f{}; size_t toRead = min<unsigned int>(DATA_SIZE, fileSize - offset);
                in.read(f.data, toRead);
                f.seq = (unsigned char)(idx % SEQ_SIZE);
                f.fileSize = 0; // 数据帧不需要
                f.offset = offset;
                f.dataLen = (unsigned short)toRead;
                f.flag = (offset + toRead >= fileSize) ? 1 : 0;
                segs.push_back(SegmentState{f});
                offset += (unsigned int)toRead; idx++;
            }
            in.close();

            cout << "[开始] 发送分片: " << segs.size() << " 个" << endl;

            int base = 0;            // 最早未确认的分片索引
            int nextIndex = 0;       // 下一个待发送的分片索引
            int total = (int)segs.size();

            while (base < total) {
                // 窗口内发送新分片
                while (nextIndex < total && nextIndex < base + WINDOW_SIZE) {
                    SegmentState &s = segs[nextIndex];
                    if (!s.sent || (s.sent && !s.acked && (double)(now() - s.lastSend) * 1000.0 / CLOCKS_PER_SEC >= TIMEOUT_MS)) {
                        sendto(sock, (char*)&s.frame, sizeof(DataFrame), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
                        s.sent = true; s.lastSend = now(); if (!s.acked) s.retries++;
                        cout << "[发送] idx=" << nextIndex << ", Seq=" << (int)s.frame.seq
                             << ", 偏移=" << s.frame.offset << ", 长度=" << s.frame.dataLen
                             << (s.frame.flag ? " [最后]" : "") << endl;
                        if (s.retries > MAX_RETRY) {
                            cout << "[错误] 分片" << nextIndex << " 重传超过上限，终止" << endl;
                            break;
                        }
                    }
                    nextIndex++;
                }

                // 接收ACK（短等待）
                // 尝试批量吸收ACK，避免丢在缓冲区
                unsigned char ackSeq;
                int ackRead = 0;
                while (recvAck(sock, ackSeq, 10)) {
                    ackRead++;
                    if (ackSeq == INFO_SEQ) continue; // 忽略信息帧ACK
                    // 在窗口内查找匹配的分片（SEQ_SIZE>=2*WINDOW，窗口内至多一个匹配）
                    for (int i = base; i < min(base + WINDOW_SIZE, total); i++) {
                        if (!segs[i].acked && segs[i].frame.seq == ackSeq) {
                            segs[i].acked = true;
                            cout << "  [确认] idx=" << i << ", Seq=" << (int)ackSeq << endl;
                            break;
                        }
                    }
                    // 滑动窗口
                    while (base < total && segs[base].acked) base++;
                    // 若本轮还可能有更多ACK，继续读，直到超时
                }

                // 对已发送未确认的进行超时重传检查
                for (int i = base; i < min(base + WINDOW_SIZE, total); i++) {
                    SegmentState &s = segs[i];
                    if (s.sent && !s.acked) {
                        double elapsed = (double)(now() - s.lastSend) * 1000.0 / CLOCKS_PER_SEC;
                        if (elapsed >= TIMEOUT_MS) {
                            sendto(sock, (char*)&s.frame, sizeof(DataFrame), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
                            s.lastSend = now(); s.retries++;
                            cout << "[重传] idx=" << i << ", Seq=" << (int)s.frame.seq << " (第" << s.retries << "次)" << endl;
                            if (s.retries > MAX_RETRY) {
                                cout << "[错误] 分片" << i << " 重传超过上限，终止" << endl; base = total; break;
                            }
                        }
                    }
                }

                // 降低CPU占用
                Sleep(1);
            }

            if (base >= total) {
                cout << "\n[完成] 文件发送完成" << endl;
            } else {
                cout << "\n[失败] 文件发送未完成" << endl;
            }
        } else {
            cout << "[错误] 未知命令" << endl;
        }
    }

    closesocket(sock);
    WSACleanup();
    cout << "客户端已关闭" << endl;
    return 0;
}
