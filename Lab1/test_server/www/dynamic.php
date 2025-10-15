<?php
// 设置Last-Modified头（固定时间，方便测试缓存）
$lastModified = gmdate('D, d M Y H:i:s', strtotime('2025-01-15 12:00:00')) . ' GMT';

// 检查客户端发送的If-Modified-Since头
if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])) {
    $ifModifiedSince = $_SERVER['HTTP_IF_MODIFIED_SINCE'];

    // 如果文件未修改，返回304
    if ($ifModifiedSince == $lastModified) {
        header('HTTP/1.1 304 Not Modified');
        header('Last-Modified: ' . $lastModified);
        exit;
    }
}

// 设置Last-Modified头
header('Last-Modified: ' . $lastModified);
header('Content-Type: text/html; charset=utf-8');

// 获取当前时间（用于验证是否是缓存）
$currentTime = date('Y-m-d H:i:s');
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PHP动态内容测试</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
            background: #f5f5f5;
        }
        .container {
            background: white;
            padding: 40px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            border-bottom: 3px solid #2196F3;
            padding-bottom: 10px;
        }
        .info-box {
            background: #e3f2fd;
            padding: 20px;
            border-left: 4px solid #2196F3;
            margin: 20px 0;
            border-radius: 5px;
        }
        .success-box {
            background: #e8f5e9;
            padding: 20px;
            border-left: 4px solid #4CAF50;
            margin: 20px 0;
            border-radius: 5px;
        }
        .time-display {
            font-size: 2em;
            color: #2196F3;
            text-align: center;
            padding: 20px;
            background: #f5f5f5;
            border-radius: 5px;
            margin: 20px 0;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }
        th, td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        th {
            background: #2196F3;
            color: white;
        }
        code {
            background: #f4f4f4;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
            color: #d63384;
        }
        .back-btn {
            display: inline-block;
            margin-top: 20px;
            padding: 10px 20px;
            background: #2196F3;
            color: white;
            text-decoration: none;
            border-radius: 5px;
        }
        .back-btn:hover {
            background: #1976D2;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔄 PHP动态内容缓存测试</h1>

        <div class="time-display">
            页面生成时间: <?php echo $currentTime; ?>
        </div>

        <div class="success-box">
            <h3>✅ 这个页面支持缓存</h3>
            <p>如果看到相同的时间，说明代理服务器正确使用了缓存！</p>
        </div>

        <h2>HTTP 头信息：</h2>
        <table>
            <tr>
                <th>头部名称</th>
                <th>值</th>
            </tr>
            <tr>
                <td><strong>Last-Modified</strong></td>
                <td><code><?php echo $lastModified; ?></code></td>
            </tr>
            <tr>
                <td><strong>If-Modified-Since</strong></td>
                <td><code><?php echo isset($_SERVER['HTTP_IF_MODIFIED_SINCE']) ? $_SERVER['HTTP_IF_MODIFIED_SINCE'] : '(未发送)'; ?></code></td>
            </tr>
            <tr>
                <td><strong>请求方法</strong></td>
                <td><code><?php echo $_SERVER['REQUEST_METHOD']; ?></code></td>
            </tr>
            <tr>
                <td><strong>User-Agent</strong></td>
                <td><code><?php echo substr($_SERVER['HTTP_USER_AGENT'], 0, 50); ?>...</code></td>
            </tr>
        </table>

        <div class="info-box">
            <h3>📝 测试说明：</h3>
            <ol>
                <li><strong>首次访问：</strong>
                    <ul>
                        <li>记录"页面生成时间"</li>
                        <li>代理服务器会保存响应到缓存</li>
                        <li>查看日志应显示：<code>[缓存] 已保存缓存</code></li>
                    </ul>
                </li>
                <li><strong>刷新页面：</strong>
                    <ul>
                        <li>如果"页面生成时间"<strong>没有变化</strong>，说明使用了缓存 ✅</li>
                        <li>代理会添加 <code>If-Modified-Since</code> 头</li>
                        <li>服务器返回 304，代理使用缓存响应</li>
                    </ul>
                </li>
                <li><strong>验证缓存：</strong>
                    <ul>
                        <li>多次刷新，时间应保持不变</li>
                        <li>查看 <code>./cache</code> 目录中的缓存文件</li>
                    </ul>
                </li>
            </ol>
        </div>

        <?php if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])): ?>
        <div class="success-box">
            <h3>🎉 检测到 If-Modified-Since 头！</h3>
            <p>代理服务器正在使用缓存验证机制。</p>
            <p>客户端发送：<code><?php echo $_SERVER['HTTP_IF_MODIFIED_SINCE']; ?></code></p>
            <p>服务器Last-Modified：<code><?php echo $lastModified; ?></code></p>
            <?php if ($_SERVER['HTTP_IF_MODIFIED_SINCE'] == $lastModified): ?>
            <p><strong style="color: #4CAF50;">✅ 匹配！服务器应返回 304 Not Modified</strong></p>
            <?php else: ?>
            <p><strong style="color: #ff9800;">⚠️ 不匹配！服务器返回 200 OK 和新内容</strong></p>
            <?php endif; ?>
        </div>
        <?php endif; ?>

        <a href="index.html" class="back-btn">← 返回首页</a>
    </div>
</body>
</html>
