/*
 * 实验4.1 - 改进版发送端程序（主机Camellya）
 * 功能：支持控制台输入多条消息并发送到转发主机
 * 编译：gcc -o send_ip_ad send_ip_ad.c
 * 运行：./send_ip_ad
 * 改进点：
 *   - 支持控制台输入消息
 *   - 支持发送多条消息
 *   - 输入 "quit" 或 "exit" 退出程序
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define DEST_IP "192.168.10.101"  // 转发主机Shorekeeper的IP
#define DEST_PORT 12345

int main() {
    int sockfd;
    struct sockaddr_in dest_addr;
    char message[BUFFER_SIZE];
    int msg_count = 0;

    // 创建 UDP 套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 1;
    }

    // 设置目标地址（转发主机的地址）
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);

    printf("===========================================\n");
    printf("  UDP 消息发送程序 - 主机 Camellya\n");
    printf("===========================================\n");
    printf("目标主机: %s:%d (Shorekeeper)\n", DEST_IP, DEST_PORT);
    printf("输入消息进行发送，输入 'quit' 或 'exit' 退出\n");
    printf("===========================================\n\n");

    // 循环读取用户输入并发送
    while (1) {
        printf("请输入消息 [%d]: ", msg_count + 1);
        fflush(stdout);

        // 读取用户输入
        if (fgets(message, BUFFER_SIZE, stdin) == NULL) {
            printf("\n读取输入失败，程序退出\n");
            break;
        }

        // 移除换行符
        size_t len = strlen(message);
        if (len > 0 && message[len - 1] == '\n') {
            message[len - 1] = '\0';
            len--;
        }

        // 检查是否退出
        if (strcmp(message, "quit") == 0 || strcmp(message, "exit") == 0) {
            printf("\n程序退出\n");
            break;
        }

        // 检查空消息
        if (len == 0) {
            printf("消息不能为空，请重新输入\n");
            continue;
        }

        // 发送数据报
        if (sendto(sockfd, message, strlen(message), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("sendto");
            continue;
        }

        msg_count++;
        printf("✓ 消息已发送 [%d]: \"%s\" (%ld 字节)\n\n",
               msg_count, message, strlen(message));

        // 短暂延迟，避免发送过快
        usleep(100000); // 0.1秒
    }

    printf("\n===========================================\n");
    printf("总共发送了 %d 条消息\n", msg_count);
    printf("===========================================\n");

    close(sockfd);
    return 0;
}
