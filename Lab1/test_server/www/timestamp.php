<?php
// 设置Last-Modified为固定时间
$lastModified = gmdate('D, d M Y H:i:s', strtotime('2025-01-15 10:00:00')) . ' GMT';

// 检查If-Modified-Since
if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])) {
    if ($_SERVER['HTTP_IF_MODIFIED_SINCE'] == $lastModified) {
        header('HTTP/1.1 304 Not Modified');
        header('Last-Modified: ' . $lastModified);
        exit;
    }
}

header('Last-Modified: ' . $lastModified);
header('Content-Type: text/html; charset=utf-8');

$generatedTime = date('Y-m-d H:i:s');
$requestCount = isset($_COOKIE['request_count']) ? intval($_COOKIE['request_count']) + 1 : 1;
setcookie('request_count', $requestCount, time() + 3600);
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>时间戳测试</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, sans-serif;
            max-width: 900px;
            margin: 30px auto;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        }
        .container {
            background: white;
            padding: 40px;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
        }
        h1 {
            color: #667eea;
            text-align: center;
            margin-bottom: 30px;
        }
        .timestamp-box {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 40px;
            border-radius: 10px;
            text-align: center;
            margin: 30px 0;
            box-shadow: 0 5px 20px rgba(0,0,0,0.2);
        }
        .timestamp-box h2 {
            font-size: 1.2em;
            margin-bottom: 10px;
            opacity: 0.9;
        }
        .timestamp-box .time {
            font-size: 3em;
            font-weight: bold;
            font-family: 'Courier New', monospace;
        }
        .info-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin: 20px 0;
        }
        .info-card {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 8px;
            border-left: 4px solid #667eea;
        }
        .info-card h3 {
            color: #667eea;
            margin-bottom: 10px;
            font-size: 1.1em;
        }
        .info-card p {
            color: #666;
            margin: 5px 0;
        }
        .status {
            padding: 10px 20px;
            border-radius: 20px;
            display: inline-block;
            font-weight: bold;
            margin: 10px 0;
        }
        .status.cached {
            background: #4CAF50;
            color: white;
        }
        .status.fresh {
            background: #2196F3;
            color: white;
        }
        .instructions {
            background: #fff3cd;
            padding: 20px;
            border-left: 4px solid #ffc107;
            border-radius: 5px;
            margin: 20px 0;
        }
        .back-btn {
            display: inline-block;
            margin-top: 20px;
            padding: 12px 30px;
            background: #667eea;
            color: white;
            text-decoration: none;
            border-radius: 5px;
            transition: background 0.3s;
        }
        .back-btn:hover {
            background: #764ba2;
        }
        code {
            background: #f4f4f4;
            padding: 3px 8px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
            color: #d63384;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>⏰ 时间戳缓存测试</h1>

        <div class="timestamp-box">
            <h2>页面生成时间</h2>
            <div class="time"><?php echo $generatedTime; ?></div>
            <p style="margin-top: 15px; opacity: 0.9;">请求次数: <?php echo $requestCount; ?></p>
        </div>

        <div class="instructions">
            <h3>🎯 如何判断是否使用了缓存？</h3>
            <p><strong>很简单：多次刷新页面</strong></p>
            <ul style="margin-top: 10px;">
                <li>如果时间<strong>始终相同</strong> → ✅ 代理正确使用缓存</li>
                <li>如果时间<strong>不断变化</strong> → ❌ 没有使用缓存或缓存失效</li>
            </ul>
        </div>

        <div class="info-grid">
            <div class="info-card">
                <h3>📤 Last-Modified</h3>
                <p><code><?php echo $lastModified; ?></code></p>
            </div>
            <div class="info-card">
                <h3>📥 If-Modified-Since</h3>
                <p><code><?php echo isset($_SERVER['HTTP_IF_MODIFIED_SINCE']) ? $_SERVER['HTTP_IF_MODIFIED_SINCE'] : '(未发送)'; ?></code></p>
            </div>
        </div>

        <?php if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])): ?>
            <?php if ($_SERVER['HTTP_IF_MODIFIED_SINCE'] == $lastModified): ?>
                <div class="status cached">
                    ✅ 缓存验证成功 - 服务器返回 304 Not Modified
                </div>
                <p style="color: #4CAF50; margin-top: 10px;">
                    <strong>如果你看到这段文字，说明代理没有正确处理304响应。</strong><br>
                    正确的行为：代理收到304后，应返回之前缓存的完整响应给客户端。
                </p>
            <?php else: ?>
                <div class="status fresh">
                    🔄 Last-Modified不匹配 - 返回新内容
                </div>
            <?php endif; ?>
        <?php else: ?>
            <div class="status fresh">
                ⚡ 首次请求或无缓存 - 返回完整响应
            </div>
        <?php endif; ?>

        <div class="instructions">
            <h3>📋 测试步骤</h3>
            <ol>
                <li><strong>首次访问：</strong>记录上方显示的时间</li>
                <li><strong>刷新页面（F5）：</strong>观察时间是否变化
                    <ul>
                        <li>时间不变 → 缓存工作正常 ✅</li>
                        <li>时间改变 → 检查代理服务器日志 ⚠️</li>
                    </ul>
                </li>
                <li><strong>查看代理日志：</strong>
                    <ul>
                        <li>首次：应显示 <code>[缓存] 未找到缓存</code></li>
                        <li>刷新：应显示 <code>[缓存] 找到缓存</code> 和 <code>[响应] 状态码: 304</code></li>
                    </ul>
                </li>
            </ol>
        </div>

        <a href="index.html" class="back-btn">← 返回首页</a>
    </div>
</body>
</html>
