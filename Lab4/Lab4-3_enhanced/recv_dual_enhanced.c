/*
 * 实验4.3增强版 - 接收与交互端程序（目标/接收主机）
 * 功能：在目标主机上接收来自源主机的UDP消息，同时支持向源主机发送消息（双向通信）
 * 编译：gcc -o recv_dual_enhanced recv_dual_enhanced.c -lpthread
 * 运行：./recv_dual_enhanced
 *
 * 说明：本程序仿照 `send_dual_enhanced.c` 的结构和日志风格实现，注释为中文
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024
#define LISTEN_PORT 12345      // 本程序监听端口（对应 send 的 DEST_PORT）
#define DEFAULT_PEER_IP "192.168.1.2" // 默认发送目标（源主机）
#define DEFAULT_PEER_PORT 54321 // 默认发送目标端口（对应 send 的 LOCAL_PORT）

// 全局控制变量
static volatile int keep_running = 1;
static int sockfd = -1;
static int msg_sent = 0;
static int msg_received = 0;

// 互斥保护最后一次接收的对端地址
static pthread_mutex_t peer_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct sockaddr_in last_peer_addr;
static int last_peer_known = 0;

// 消息结构，与 send_dual_enhanced.c 保持一致
struct message {
	int seq_num;
	struct timeval timestamp;
	char content[BUFFER_SIZE - sizeof(int) - sizeof(struct timeval)];
};

// 信号处理，优雅退出
void signal_handler(int signum) {
	(void)signum;
	printf("\n接收到退出信号，正在终止...\n");
	keep_running = 0;
}

// 获取当前时间字符串（用于日志）
void get_time_string(char *time_str, size_t size) {
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	strftime(time_str, size, "%H:%M:%S", t);
}

// 计算时间差（毫秒）
long time_diff_ms(struct timeval *start, struct timeval *end) {
	return (end->tv_sec - start->tv_sec) * 1000 +
		   (end->tv_usec - start->tv_usec) / 1000;
}

// 接收线程：持续监听 LISTEN_PORT 并打印收到的消息，同时更新 last_peer_addr
void *receive_thread(void *arg) {
	(void)arg;
	struct sockaddr_in from_addr;
	socklen_t addr_len = sizeof(from_addr);
	char buffer[BUFFER_SIZE];
	char time_str[20];

	printf("[接收线程] 已启动，监听端口 %d\n", LISTEN_PORT);
	printf("[接收线程] 等待接收来自对端的消息...\n\n");

	while (keep_running) {
		memset(buffer, 0, sizeof(buffer));

		// 设置接收超时以便能响应退出信号
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

		int recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
							   (struct sockaddr *)&from_addr, &addr_len);

		if (recv_len < 0) {
			continue; // 超时或错误，检查 keep_running 后继续
		}

		// 解析消息并打印
		struct message *msg = (struct message *)buffer;
		struct timeval now;
		gettimeofday(&now, NULL);
		long rtt = time_diff_ms(&msg->timestamp, &now);

		msg_received++;
		get_time_string(time_str, sizeof(time_str));

		printf("\n╔═══════════════════════════════════════════════════════════╗\n");
		printf("║ [%s] 收到消息 #%-4d                                 ║\n", time_str, msg_received);
		printf("╠═══════════════════════════════════════════════════════════╣\n");
		printf("║ 来源:        %-44s ║\n", inet_ntoa(from_addr.sin_addr));
		printf("║ 端口:        %-44d ║\n", ntohs(from_addr.sin_port));
		printf("║ 序列号:      %-44d ║\n", msg->seq_num);
		printf("║ 时延(接收时):  %-40ld ms║\n", rtt);
		printf("║ 内容:        %-44s ║\n", msg->content);
		printf("╚═══════════════════════════════════════════════════════════╝\n\n");

		// 记录最后一个对端地址，供用户发送使用
		pthread_mutex_lock(&peer_mutex);
		last_peer_addr = from_addr;
		last_peer_known = 1;
		pthread_mutex_unlock(&peer_mutex);
	}

	printf("[接收线程] 退出\n");
	return NULL;
}

int main() {
	struct sockaddr_in local_addr;
	struct sockaddr_in peer_addr;
	char input[BUFFER_SIZE];
	pthread_t recv_thread;
	char time_str[20];

	printf("╔═══════════════════════════════════════════════════════════╗\n");
	printf("║        UDP 双向通信程序 - 目标/接收主机 (增强版)          ║\n");
	printf("║              (实验4.3 Enhanced)                           ║\n");
	printf("╚═══════════════════════════════════════════════════════════╝\n\n");
	printf("本程序监听端口: %d，默认发送目标: %s:%d\n\n", LISTEN_PORT, DEFAULT_PEER_IP, DEFAULT_PEER_PORT);

	// 注册信号处理
	signal(SIGINT, signal_handler);

	// 创建 UDP 套接字
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket");
		return 1;
	}

	// 绑定本地地址（用于接收）
	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(LISTEN_PORT);
	local_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
		perror("bind");
		close(sockfd);
		return 1;
	}

	// 初始化默认对端地址（可以通过接收更新）
	memset(&peer_addr, 0, sizeof(peer_addr));
	peer_addr.sin_family = AF_INET;
	peer_addr.sin_port = htons(DEFAULT_PEER_PORT);
	peer_addr.sin_addr.s_addr = inet_addr(DEFAULT_PEER_IP);

	pthread_mutex_lock(&peer_mutex);
	last_peer_addr = peer_addr;
	last_peer_known = 1; // 默认已知
	pthread_mutex_unlock(&peer_mutex);

	// 创建接收线程
	if (pthread_create(&recv_thread, NULL, receive_thread, NULL) != 0) {
		perror("pthread_create");
		close(sockfd);
		return 1;
	}

	printf("输入消息进行发送，输入 'quit' 或 'exit' 退出\n");
	printf("如果需要向不同目标发送，请输入: /peer <ip> <port>\n");
	printf("===========================================\n\n");

	// 主循环：读取用户输入并发送到 last_peer_addr
	while (keep_running) {
        sleep(0.6);
		printf("请输入消息 [%d]: ", msg_sent + 1);
		fflush(stdout);

		if (fgets(input, sizeof(input), stdin) == NULL) {
			if (keep_running) {
				printf("\n读取输入失败\n");
			}
			break;
		}

		// 去掉换行
		size_t len = strlen(input);
		if (len > 0 && input[len - 1] == '\n') {
			input[len - 1] = '\0';
			len--;
		}

		// 退出条件
		if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
			printf("准备退出...\n");
			break;
		}

		// 支持设置对端: /peer <ip> <port>
		if (strncmp(input, "/peer ", 6) == 0) {
			char ipbuf[64];
			int port = 0;
			if (sscanf(input + 6, "%63s %d", ipbuf, &port) == 2) {
				pthread_mutex_lock(&peer_mutex);
				last_peer_addr.sin_family = AF_INET;
				last_peer_addr.sin_port = htons(port);
				last_peer_addr.sin_addr.s_addr = inet_addr(ipbuf);
				last_peer_known = 1;
				pthread_mutex_unlock(&peer_mutex);
				printf("已设置发送目标为 %s:%d\n\n", ipbuf, port);
				continue;
			} else {
				printf("/peer 用法: /peer <ip> <port>\n\n");
				continue;
			}
		}

		if (len == 0) {
			printf("消息不能为空，请重新输入\n\n");
			continue;
		}

		// 构造消息
		struct message msg;
		msg.seq_num = msg_sent + 1;
		gettimeofday(&msg.timestamp, NULL);
		strncpy(msg.content, input, sizeof(msg.content) - 1);
		msg.content[sizeof(msg.content) - 1] = '\0';

		// 读取当前对端地址拷贝
		pthread_mutex_lock(&peer_mutex);
		struct sockaddr_in target = last_peer_addr;
		int known = last_peer_known;
		pthread_mutex_unlock(&peer_mutex);

		if (!known) {
			printf("当前没有已知的对端地址，请先等待接收或使用 /peer 设置目标\n\n");
			continue;
		}

		// 发送数据
		if (sendto(sockfd, &msg, sizeof(msg), 0,
				   (struct sockaddr *)&target, sizeof(target)) < 0) {
			perror("sendto");
			continue;
		}

		msg_sent++;
		get_time_string(time_str, sizeof(time_str));

		printf("┌───────────────────────────────────────────────────────────┐\n");
		printf("│ [%s] ✓ 已发送消息 #%-4d                             │\n", time_str, msg_sent);
		printf("├───────────────────────────────────────────────────────────┤\n");
		printf("│ 目标:        %s:%-30d   │\n", inet_ntoa(target.sin_addr), ntohs(target.sin_port));
		printf("│ 序列号:      %-44d │\n", msg.seq_num);
		printf("│ 内容:        %-44s │\n", msg.content);
		printf("│ 长度:        %-41ld字节│\n", sizeof(msg));
		printf("└───────────────────────────────────────────────────────────┘\n\n");

		// 小延迟
		usleep(100000);
	}

	// 结束
	keep_running = 0;
	printf("\n等待接收线程结束...\n");
	pthread_join(recv_thread, NULL);

	printf("\n╔═══════════════════════════════════════════════════════════╗\n");
	printf("║                    通信统计信息                           ║\n");
	printf("╠═══════════════════════════════════════════════════════════╣\n");
	printf("║ 发送消息数:   %-42d  ║\n", msg_sent);
	printf("║ 接收消息数:   %-42d  ║\n", msg_received);
	// printf("║ 丢包率:       %-39.1f%% ║\n",
	// 	   msg_sent > 0 ? (1.0 - (double)msg_received / msg_sent) * 100 : 0.0);
	printf("╚═══════════════════════════════════════════════════════════╝\n");

	close(sockfd);
	return 0;
}

