#ifndef FILTER_MANAGER_H
#define FILTER_MANAGER_H

#include "http_structs.h"
#include <vector>
#include <string>
#include <map>

// 全局过滤规则
extern std::vector<FilterRule> websiteRules;  // 网站过滤规则
extern std::vector<FilterRule> userRules;     // 用户过滤规则
extern std::map<std::string, std::string> redirectRules; // 网站引导规则

// 加载过滤规则
void LoadFilterRules();

// 检查网站访问权限（支持URL路径过滤和端口匹配）
BOOL CheckWebsiteAccess(const char *hostname, int port, const char *url);

// 检查用户访问权限
BOOL CheckUserAccess(const char *clientIP);

// 获取重定向目标
BOOL GetRedirectTarget(const char *hostname, char *redirectHost, int *redirectPort);

// 发送阻止响应
void SendBlockedResponse(SOCKET clientSocket);

#endif // FILTER_MANAGER_H
