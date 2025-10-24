#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>
#include <ctime>
#include <unordered_set>
#include <unordered_map>
#include <direct.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 配置
#define SERVER_PORT 12340
#define DATA_SIZE 1020
#define SEQ_SIZE 32             // 序列号空间大小 (需 >= 2*窗口大小)
#define RECV_ACK_LOSS 0.2f      // 模拟ACK丢失率（可按需修改）

// 数据帧结构（与客户端保持一致）
struct DataFrame {
    unsigned char seq;          // 序列号 0..SEQ_SIZE-1
    unsigned int fileSize;      // 文件总大小（仅信息帧使用）
    unsigned int offset;        // 数据在文件中的偏移
    unsigned short dataLen;     // 数据长度
    char data[DATA_SIZE];       // 数据
    unsigned char flag;         // 0=数据帧, 1=最后一帧, 2=ACK, 4=信息帧
};

// 生成[0,1)随机数
static inline float frand() {
    return (float)rand() / (float)RAND_MAX;
}

// 发送ACK
void sendAck(SOCKET sock, sockaddr_in &client, unsigned char seq) {
    // 模拟ACK丢失
    if (frand() < RECV_ACK_LOSS) {
        cout << "  [丢失] 模拟ACK=" << (int)seq << " 丢失" << endl;
        return;
    }
    DataFrame ack{};
    ack.seq = seq;
    ack.flag = 2;
    ack.dataLen = 0;
    sendto(sock, (char*)&ack, sizeof(DataFrame), 0, (sockaddr*)&client, sizeof(client));
    cout << "  [发送] ACK=" << (int)seq << endl;
}

int main() {
    srand((unsigned int)time(NULL));

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup失败" << endl;
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server == INVALID_SOCKET) {
        cout << "创建socket失败" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cout << "绑定失败" << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    }

    cout << "========================================" << endl;
    cout << "SR 文件传输服务器" << endl;
    cout << "端口: " << SERVER_PORT << endl;
    cout << "数据帧大小: " << DATA_SIZE << endl;
    cout << "ACK丢包率: " << (RECV_ACK_LOSS * 100) << "%" << endl;
    cout << "========================================\n" << endl;

    _mkdir("received_files");

    // 当前传输会话状态
    bool receiving = false;
    string savePath;
    ofstream out;
    unsigned int totalSize = 0;
    unsigned int lastChunkIndex = 0; // 最后一个分片索引
    unordered_set<unsigned int> receivedChunks; // 已写入的分片索引
    bool completionGrace = false;     // 完成后宽限期内继续回ACK，帮助发送端收尾
    clock_t completionTime = 0;       // 完成时刻

    sockaddr_in client{};
    int clientLen = sizeof(client);

    // 设置接收超时（30秒）
    DWORD rcvTimeout = 30000;
    setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (char*)&rcvTimeout, sizeof(rcvTimeout));

    while (true) {
        DataFrame frame{};
        int len = recvfrom(server, (char*)&frame, sizeof(frame), 0, (sockaddr*)&client, &clientLen);

        if (len > 0) {
            if (frame.flag == 4) {
                // 文件信息帧
                string fileName(frame.data, frame.dataLen);
                cout << "\n[请求] 上传文件: " << fileName << ", 大小=" << frame.fileSize << " 字节" << endl;

                // 若正在接收，关闭上一次
                if (receiving) {
                    out.close();
                    receiving = false;
                    receivedChunks.clear();
                }

                // 初始化会话
                _mkdir("received_files");
                savePath = string("received_files/") + fileName;
                out.open(savePath, ios::binary | ios::trunc);
                if (!out.is_open()) {
                    cout << "[错误] 无法创建文件: " << savePath << endl;
                    // 也回ACK让客户端超时重试或失败
                    sendAck(server, client, frame.seq);
                    continue;
                }

                totalSize = frame.fileSize;
                lastChunkIndex = (totalSize + DATA_SIZE - 1) / DATA_SIZE - 1;
                receivedChunks.clear();
                receiving = true;
                completionGrace = false; // 新会话开始，取消上次宽限

                // 回ACK确认信息帧
                sendAck(server, client, frame.seq);
                cout << "[确认] 已准备接收: " << savePath << endl;
                continue;
            }

            // 非ACK帧且在接收状态下处理数据
            if (receiving && (frame.flag == 0 || frame.flag == 1)) {
                unsigned int chunkIndex = frame.offset / DATA_SIZE;

                cout << "[接收] Seq=" << (int)frame.seq
                     << ", 偏移=" << frame.offset
                     << ", 长度=" << frame.dataLen
                     << (frame.flag == 1 ? " [最后]" : "")
                     << endl;

                // 对每个数据帧都发送ACK（SR选择确认）
                sendAck(server, client, frame.seq);

                // 去重写入
                if (receivedChunks.find(chunkIndex) == receivedChunks.end()) {
                    // 定位并写入
                    out.seekp(frame.offset, ios::beg);
                    out.write(frame.data, frame.dataLen);
                    receivedChunks.insert(chunkIndex);
                } else {
                    cout << "  [重复] 已接收过该分片，忽略写入" << endl;
                }

                // 完成判定：已接收分片数达到应有分片数
                unsigned int needChunks = (totalSize + DATA_SIZE - 1) / DATA_SIZE;
                if (receivedChunks.size() >= needChunks) {
                    out.flush();
                    out.close();
                    receiving = false;
                    // 进入完成后的ACK宽限期（例如3秒）
                    completionGrace = true;
                    completionTime = clock();
                    cout << "\n[完成] 文件接收完成! 大小=" << totalSize
                         << " 字节, 分片数=" << needChunks << endl;
                    cout << "[保存] " << savePath << endl;
                }
                continue;
            }

            // 已完成后的宽限期内：对重复到来的数据帧继续回ACK，帮助客户端收尾
            if (!receiving && completionGrace && (frame.flag == 0 || frame.flag == 1)) {
                sendAck(server, client, frame.seq);
                cout << "  [完成后ACK] Seq=" << (int)frame.seq << endl;
                continue;
            }

            // 其他情况：可能是重复的信息帧、乱序控制等，尽量保持幂等
            if (frame.flag == 2) {
                // 收到ACK（本服务器不发送数据，这里忽略）
                continue;
            }

            // 收到未知帧，忽略
        } else if (len == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                if (receiving) {
                    cout << "[超时] 等待数据超时，当前进度: "
                         << receivedChunks.size()*DATA_SIZE << "/" << totalSize << endl;
                }
                // 宽限期到期检测（3秒）
                if (completionGrace) {
                    double elapsedMs = (double)(clock() - completionTime) * 1000.0 / CLOCKS_PER_SEC;
                    if (elapsedMs >= 3000.0) {
                        completionGrace = false;
                        cout << "[结束] 完成后ACK宽限期结束" << endl;
                    }
                }
                // 继续等待新的请求
            } else {
                cout << "[错误] 接收失败: " << err << endl;
            }
        }
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
