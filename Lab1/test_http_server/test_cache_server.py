#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HTTP 缓存测试服务器
支持 Last-Modified 和 If-Modified-Since 头部，用于测试代理服务器的缓存功能
"""

import http.server
import socketserver
import os
from datetime import datetime
from email.utils import formatdate, parsedate_to_datetime
import time

PORT = 80

class CacheTestHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    """支持缓存验证的 HTTP 请求处理器"""

    def end_headers(self):
        """添加缓存相关的响应头"""
        # 允许跨域（可选）
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

    def do_GET(self):
        """处理 GET 请求，支持 If-Modified-Since 验证"""

        # 获取请求的文件路径
        path = self.translate_path(self.path)

        # 打印请求信息
        print(f"\n{'='*60}")
        print(f"[请求] {self.command} {self.path}")
        print(f"[时间] {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

        # 检查是否有 If-Modified-Since 头部
        if_modified_since = self.headers.get('If-Modified-Since')
        if if_modified_since:
            print(f"[缓存验证] 收到 If-Modified-Since: {if_modified_since}")

        # 检查文件是否存在
        if not os.path.exists(path):
            print(f"[错误] 文件不存在: {path}")
            self.send_error(404, "File not found")
            return

        if os.path.isdir(path):
            # 目录，使用默认处理
            super().do_GET()
            return

        # 获取文件的最后修改时间
        file_stat = os.stat(path)
        last_modified_timestamp = file_stat.st_mtime
        last_modified = formatdate(last_modified_timestamp, usegmt=True)

        print(f"[文件信息] 路径: {path}")
        print(f"[文件信息] 大小: {file_stat.st_size} 字节")
        print(f"[文件信息] Last-Modified: {last_modified}")

        # 如果客户端提供了 If-Modified-Since，进行比较
        if if_modified_since:
            try:
                # 解析客户端提供的时间
                client_time = parsedate_to_datetime(if_modified_since)
                # 解析文件的修改时间
                file_time = parsedate_to_datetime(last_modified)

                # 比较时间（忽略毫秒差异）
                if abs((file_time - client_time).total_seconds()) < 1:
                    print(f"[304] 文件未修改，返回 304 Not Modified")
                    print("="*60)
                    # 文件未修改，返回 304
                    self.send_response(304)
                    self.send_header('Last-Modified', last_modified)
                    self.send_header('Cache-Control', 'max-age=3600')
                    self.end_headers()
                    return
                else:
                    print(f"[200] 文件已修改，返回新内容")
            except Exception as e:
                print(f"[警告] 解析 If-Modified-Since 失败: {e}")
        else:
            print(f"[200] 首次请求，返回完整内容")

        print("="*60)

        # 文件已修改或首次请求，返回完整内容
        try:
            with open(path, 'rb') as f:
                content = f.read()

            self.send_response(200)
            self.send_header('Content-Type', self.guess_type(path))
            self.send_header('Content-Length', str(len(content)))
            self.send_header('Last-Modified', last_modified)
            self.send_header('Cache-Control', 'max-age=3600')  # 缓存 1 小时
            self.end_headers()
            self.wfile.write(content)

        except Exception as e:
            print(f"[错误] 读取文件失败: {e}")
            self.send_error(500, f"Internal Server Error: {str(e)}")

    def log_message(self, format, *args):
        """自定义日志输出（减少噪音）"""
        # 只在我们的自定义日志中输出
        pass


def run_server():
    """启动测试服务器"""
    print("="*60)
    print("HTTP 缓存测试服务器")
    print("="*60)
    print(f"监听端口: {PORT}")
    print(f"工作目录: {os.getcwd()}")
    print(f"启动时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("\n功能说明:")
    print("  ✓ 支持 Last-Modified 响应头")
    print("  ✓ 支持 If-Modified-Since 请求头验证")
    print("  ✓ 自动返回 304 Not Modified（当文件未修改时）")
    print("\n测试页面:")
    print(f"  http://127.0.0.1:{PORT}/cache_test.html")
    print(f"  http://127.0.0.1:{PORT}/index.html")
    print("\n按 Ctrl+C 停止服务器")
    print("="*60)
    print()

    # 创建服务器
    with socketserver.TCPServer(("", PORT), CacheTestHTTPRequestHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n\n服务器已停止")


if __name__ == "__main__":
    run_server()
