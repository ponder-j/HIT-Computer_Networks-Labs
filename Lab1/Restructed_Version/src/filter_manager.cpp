#include "../include/filter_manager.h"
#include <stdio.h>
#include <string.h>

// 全局过滤规则
std::vector<FilterRule> websiteRules;  // 网站过滤规则
std::vector<FilterRule> userRules;     // 用户过滤规则
std::map<std::string, std::string> redirectRules; // 网站引导规则

// 加载过滤规则
void LoadFilterRules() {
    FILE *file = NULL;
    char line[512];

    // 加载网站过滤规则（website_filter.txt）
    fopen_s(&file, "website_filter.txt", "r");
    if (file != NULL) {
        printf("[配置] 加载网站过滤规则...\n");
        while (fgets(line, sizeof(line), file)) {
            // 跳过注释和空行
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

            // 去除换行符
            line[strcspn(line, "\r\n")] = 0;

            FilterRule rule;
            char action[16];
            char hostname[256];
            char urlPath[512] = {0};

            // 解析格式: allow/deny hostname [url_path]
            int parsed = sscanf_s(line, "%s %s %s",
                action, (unsigned)sizeof(action),
                hostname, (unsigned)sizeof(hostname),
                urlPath, (unsigned)sizeof(urlPath));

            if (parsed >= 2) {
                rule.hostname = hostname;
                rule.isAllowed = (strcmp(action, "allow") == 0);
                if (parsed == 3) {
                    rule.urlPath = urlPath;
                    printf("[配置]   %s %s%s\n", action, hostname, urlPath);
                } else {
                    rule.urlPath = "";
                    printf("[配置]   %s %s\n", action, hostname);
                }
                websiteRules.push_back(rule);
            }
        }
        fclose(file);
    } else {
        printf("[配置] 未找到 website_filter.txt，跳过网站过滤\n");
    }

    // 加载用户过滤规则（user_filter.txt）
    fopen_s(&file, "user_filter.txt", "r");
    if (file != NULL) {
        printf("[配置] 加载用户过滤规则...\n");
        while (fgets(line, sizeof(line), file)) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            line[strcspn(line, "\r\n")] = 0;

            FilterRule rule;
            char action[16];
            char ip[32];

            // 解析格式: allow/deny IP
            if (sscanf_s(line, "%s %s", action, (unsigned)sizeof(action), ip, (unsigned)sizeof(ip)) == 2) {
                rule.clientIP = ip;
                rule.isAllowed = (strcmp(action, "allow") == 0);
                userRules.push_back(rule);
                printf("[配置]   %s %s\n", action, ip);
            }
        }
        fclose(file);
    } else {
        printf("[配置] 未找到 user_filter.txt，跳过用户过滤\n");
    }

    // 加载网站引导规则（redirect.txt）
    fopen_s(&file, "redirect.txt", "r");
    if (file != NULL) {
        printf("[配置] 加载网站引导规则...\n");
        while (fgets(line, sizeof(line), file)) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            line[strcspn(line, "\r\n")] = 0;

            char source[256];
            char target[256];

            // 解析格式: source_host target_host
            if (sscanf_s(line, "%s %s", source, (unsigned)sizeof(source), target, (unsigned)sizeof(target)) == 2) {
                redirectRules[source] = target;
                printf("[配置]   %s -> %s\n", source, target);
            }
        }
        fclose(file);
    } else {
        printf("[配置] 未找到 redirect.txt，跳过网站引导\n");
    }
}

// 检查网站访问权限（支持URL路径过滤和端口匹配）
BOOL CheckWebsiteAccess(const char *hostname, int port, const char *url) {
    // 如果没有规则，默认允许
    if (websiteRules.empty()) return TRUE;

    // 构造完整的 host:port 字符串用于匹配
    char fullHost[1280];
    if (port != 80) {
        sprintf_s(fullHost, sizeof(fullHost), "%s:%d", hostname, port);
    } else {
        strcpy_s(fullHost, sizeof(fullHost), hostname);
    }

    // 遍历规则查找匹配
    for (size_t i = 0; i < websiteRules.size(); i++) {
        const char *ruleHost = websiteRules[i].hostname.c_str();

        // 检查主机名是否匹配（支持子串匹配）
        // 优先匹配完整的 host:port，如果失败则尝试只匹配 hostname
        bool hostMatched = false;

        // 情况1: 规则包含端口 (如 "165.99.42.83:30000")
        if (strchr(ruleHost, ':') != NULL) {
            // 必须完整匹配 host:port
            hostMatched = (strstr(fullHost, ruleHost) != NULL);
        }
        // 情况2: 规则只有主机名 (如 "165.99.42.83" 或 "baidu.com")
        else {
            // 只匹配主机名部分
            hostMatched = (strstr(hostname, ruleHost) != NULL);
        }

        if (hostMatched) {
            // 如果规则指定了URL路径，需要同时匹配URL
            if (!websiteRules[i].urlPath.empty()) {
                // 检查URL路径是否匹配
                if (strstr(url, websiteRules[i].urlPath.c_str()) != NULL) {
                    return websiteRules[i].isAllowed;
                }
                // URL不匹配，继续检查下一条规则
                continue;
            }
            // 只匹配主机名，不限制URL
            return websiteRules[i].isAllowed;
        }
    }

    // 默认行为：如果有规则但没匹配，允许访问
    return TRUE;
}

// 检查用户访问权限
BOOL CheckUserAccess(const char *clientIP) {
    // 如果没有规则，默认允许
    if (userRules.empty()) return TRUE;

    // 遍历规则查找匹配
    for (size_t i = 0; i < userRules.size(); i++) {
        if (strcmp(clientIP, userRules[i].clientIP.c_str()) == 0) {
            return userRules[i].isAllowed;
        }
    }

    // 默认行为：如果有规则但没匹配，允许访问
    return TRUE;
}

// 获取重定向目标
BOOL GetRedirectTarget(const char *hostname, char *redirectHost, int *redirectPort) {
    std::string host(hostname);

    if (redirectRules.find(host) != redirectRules.end()) {
        std::string target = redirectRules[host];

        // 解析目标（支持 host:port 格式）
        size_t colonPos = target.find(':');
        if (colonPos != std::string::npos) {
            strcpy_s(redirectHost, 1024, target.substr(0, colonPos).c_str());
            *redirectPort = atoi(target.substr(colonPos + 1).c_str());
        } else {
            strcpy_s(redirectHost, 1024, target.c_str());
            *redirectPort = 80;
        }
        return TRUE;
    }

    return FALSE;
}

// 发送阻止响应
void SendBlockedResponse(SOCKET clientSocket) {
    char response[1024];
    sprintf_s(response, sizeof(response),
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        strlen(BLOCKED_PAGE_HTML),
        BLOCKED_PAGE_HTML);

    send(clientSocket, response, strlen(response), 0);
}
