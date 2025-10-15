<?php
// 这个页面故意不发送Last-Modified头，测试代理是否正确处理无缓存情况
header('Content-Type: text/html; charset=utf-8');
header('Cache-Control: no-cache, no-store, must-revalidate');
header('Pragma: no-cache');
header('Expires: 0');

$currentTime = date('Y-m-d H:i:s');
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>无缓存页面测试</title>
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
            color: #f44336;
            border-bottom: 3px solid #f44336;
            padding-bottom: 10px;
        }
        .warning-box {
            background: #ffebee;
            padding: 20px;
            border-left: 4px solid #f44336;
            margin: 20px 0;
            border-radius: 5px;
        }
        .time-display {
            font-size: 2em;
            color: #f44336;
            text-align: center;
            padding: 20px;
            background: #ffebee;
            border-radius: 5px;
            margin: 20px 0;
            font-family: 'Courier New', monospace;
        }
        .info-table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }
        .info-table th,
        .info-table td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        .info-table th {
            background: #f44336;
            color: white;
        }
        code {
            background: #f4f4f4;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
        }
        .back-btn {
            display: inline-block;
            margin-top: 20px;
            padding: 10px 20px;
            background: #f44336;
            color: white;
            text-decoration: none;
            border-radius: 5px;
        }
        .back-btn:hover {
            background: #d32f2f;
        }
        .badge {
            display: inline-block;
            padding: 5px 15px;
            border-radius: 15px;
            font-size: 0.9em;
            margin: 5px;
        }
        .badge.no {
            background: #f44336;
            color: white;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚫 无缓存页面测试</h1>

        <div class="time-display">
            当前时间: <?php echo $currentTime; ?>
        </div>

        <div class="warning-box">
            <h3>⚠️ 这个页面不支持缓存</h3>
            <p>这个页面<strong>没有发送 Last-Modified 头</strong>，因此代理服务器不应该缓存它。</p>
            <p><strong>预期行为：</strong>每次刷新页面，时间都会更新！</p>
        </div>

        <h2>HTTP 响应头：</h2>
        <table class="info-table">
            <tr>
                <th>头部名称</th>
                <th>值</th>
                <th>说明</th>
            </tr>
            <tr>
                <td><strong>Last-Modified</strong></td>
                <td><span class="badge no">未设置</span></td>
                <td>没有此头，代理不应缓存</td>
            </tr>
            <tr>
                <td><strong>Cache-Control</strong></td>
                <td><code>no-cache, no-store, must-revalidate</code></td>
                <td>明确禁止缓存</td>
            </tr>
            <tr>
                <td><strong>Pragma</strong></td>
                <td><code>no-cache</code></td>
                <td>HTTP/1.0 兼容性</td>
            </tr>
            <tr>
                <td><strong>Expires</strong></td>
                <td><code>0</code></td>
                <td>立即过期</td>
            </tr>
        </table>

        <h2>测试说明：</h2>
        <ol>
            <li><strong>首次访问：</strong>记录上方显示的时间</li>
            <li><strong>刷新页面：</strong>时间应该<strong>发生变化</strong>
                <ul>
                    <li>✅ 时间变化 → 代理正确处理（没有缓存此页面）</li>
                    <li>❌ 时间不变 → 代理错误缓存了不应缓存的内容</li>
                </ul>
            </li>
            <li><strong>查看代理日志：</strong>
                <ul>
                    <li>应显示：<code>[缓存] 响应无Last-Modified头，不缓存</code></li>
                    <li>或者：代理只是转发，不进行任何缓存操作</li>
                </ul>
            </li>
        </ol>

        <div class="warning-box">
            <h3>📝 为什么要测试这个？</h3>
            <p>缓存代理服务器需要能够区分哪些内容可以缓存，哪些不能。</p>
            <p>根据实验要求，只有包含 <code>Last-Modified</code> 头的响应才应该被缓存。</p>
            <p>这个测试验证了代理服务器<strong>不会错误地缓存不应缓存的内容</strong>。</p>
        </div>

        <h2>与缓存页面对比：</h2>
        <table class="info-table">
            <tr>
                <th>页面</th>
                <th>Last-Modified</th>
                <th>刷新后时间</th>
            </tr>
            <tr>
                <td>dynamic.php</td>
                <td>✅ 有</td>
                <td>不变（使用缓存）</td>
            </tr>
            <tr>
                <td>timestamp.php</td>
                <td>✅ 有</td>
                <td>不变（使用缓存）</td>
            </tr>
            <tr>
                <td><strong>no-cache.php</strong></td>
                <td><strong>❌ 无</strong></td>
                <td><strong>改变（不使用缓存）</strong></td>
            </tr>
        </table>

        <a href="index.html" class="back-btn">← 返回首页</a>
    </div>
</body>
</html>
