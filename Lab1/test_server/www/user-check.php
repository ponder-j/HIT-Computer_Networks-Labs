<?php
// 获取客户端IP
$clientIP = $_SERVER['REMOTE_ADDR'];
$forwardedFor = isset($_SERVER['HTTP_X_FORWARDED_FOR']) ? $_SERVER['HTTP_X_FORWARDED_FOR'] : 'None';
$userAgent = $_SERVER['HTTP_USER_AGENT'];
$requestTime = date('Y-m-d H:i:s');

// 设置Last-Modified用于缓存测试
$lastModified = gmdate('D, d M Y H:i:s', time() - 3600) . ' GMT';
header('Last-Modified: ' . $lastModified);
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>用户过滤测试 - IP检查</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            margin: 0;
            padding: 20px;
        }

        .container {
            background: white;
            padding: 50px;
            border-radius: 20px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 700px;
            width: 100%;
        }

        h1 {
            color: #667eea;
            text-align: center;
            margin-bottom: 30px;
        }

        .ip-display {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 15px;
            text-align: center;
            margin-bottom: 30px;
        }

        .ip-display h2 {
            margin: 0 0 10px 0;
            font-size: 2.5em;
        }

        .ip-display p {
            margin: 0;
            opacity: 0.9;
        }

        .info-table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }

        .info-table th,
        .info-table td {
            padding: 15px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }

        .info-table th {
            background: #f5f5f5;
            color: #333;
            font-weight: bold;
            width: 40%;
        }

        .info-table td {
            word-break: break-all;
        }

        .status-box {
            background: #e8f5e9;
            border-left: 4px solid #4CAF50;
            padding: 20px;
            margin: 20px 0;
            border-radius: 5px;
        }

        .status-box h3 {
            color: #2e7d32;
            margin-top: 0;
        }

        .warning-box {
            background: #fff3cd;
            border-left: 4px solid #ffc107;
            padding: 20px;
            margin: 20px 0;
            border-radius: 5px;
        }

        .warning-box h3 {
            color: #856404;
            margin-top: 0;
        }

        code {
            background: #f4f4f4;
            padding: 3px 8px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
            color: #d63384;
        }

        .back-btn {
            display: inline-block;
            margin-top: 20px;
            padding: 15px 40px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            text-decoration: none;
            border-radius: 25px;
            font-weight: bold;
            text-align: center;
        }

        .back-btn:hover {
            opacity: 0.9;
        }

        .test-btn {
            display: inline-block;
            margin: 10px 5px;
            padding: 12px 25px;
            background: #4CAF50;
            color: white;
            text-decoration: none;
            border-radius: 20px;
        }

        .test-btn:hover {
            background: #45a049;
        }

        .btn-container {
            text-align: center;
            margin-top: 30px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>👤 用户过滤测试 - IP检查</h1>

        <div class="ip-display">
            <h2><?php echo $clientIP; ?></h2>
            <p>你的IP地址</p>
        </div>

        <div class="status-box">
            <h3>✅ 访问状态：允许</h3>
            <p>如果你能看到这个页面，说明你的IP地址<strong>未被阻止</strong>。</p>
            <p>代理服务器允许来自 <code><?php echo $clientIP; ?></code> 的访问请求。</p>
        </div>

        <h2 style="color: #667eea; margin-top: 30px;">📊 连接信息</h2>
        <table class="info-table">
            <tr>
                <th>客户端IP</th>
                <td><?php echo $clientIP; ?></td>
            </tr>
            <tr>
                <th>X-Forwarded-For</th>
                <td><?php echo $forwardedFor; ?></td>
            </tr>
            <tr>
                <th>User-Agent</th>
                <td><?php echo $userAgent; ?></td>
            </tr>
            <tr>
                <th>请求时间</th>
                <td><?php echo $requestTime; ?></td>
            </tr>
            <tr>
                <th>服务器</th>
                <td><?php echo $_SERVER['SERVER_SOFTWARE']; ?></td>
            </tr>
            <tr>
                <th>协议</th>
                <td><?php echo $_SERVER['SERVER_PROTOCOL']; ?></td>
            </tr>
        </table>

        <div class="warning-box">
            <h3>🧪 如何测试用户过滤</h3>
            <p><strong>测试被阻止的情况：</strong></p>
            <ol>
                <li>编辑 <code>user_filter.txt</code>，添加：<code>deny <?php echo $clientIP; ?></code></li>
                <li>重启 ProxyServerAdvanced.exe</li>
                <li>刷新此页面</li>
                <li>应该看到 <strong>403 Forbidden</strong> 错误</li>
            </ol>

            <p style="margin-top: 15px;"><strong>测试允许的情况（当前状态）：</strong></p>
            <ul>
                <li>配置文件中没有阻止规则，或</li>
                <li>明确配置了：<code>allow <?php echo $clientIP; ?></code></li>
            </ul>
        </div>

        <h2 style="color: #667eea; margin-top: 30px;">📝 配置示例</h2>
        <div style="background: #f5f5f5; padding: 20px; border-radius: 10px;">
            <p><strong>user_filter.txt - 阻止此IP：</strong></p>
            <pre style="background: white; padding: 15px; border-radius: 5px; overflow-x: auto;">deny <?php echo $clientIP; ?>
allow 192.168.1.100</pre>

            <p style="margin-top: 15px;"><strong>代理日志应显示：</strong></p>
            <pre style="background: white; padding: 15px; border-radius: 5px; overflow-x: auto;">[新连接] 客户端: <?php echo $clientIP; ?>:xxxxx
[用户过滤] 阻止用户访问: <?php echo $clientIP; ?></pre>
        </div>

        <div class="btn-container">
            <a href="user-status.php" class="test-btn">查看访问状态</a>
            <a href="index.html" class="back-btn">← 返回测试中心</a>
        </div>

        <p style="margin-top: 30px; text-align: center; color: #999; font-size: 0.9em;">
            测试时间：<?php echo $requestTime; ?>
        </p>
    </div>

    <script>
        console.log('=== 用户过滤测试 ===');
        console.log('客户端IP:', '<?php echo $clientIP; ?>');
        console.log('访问状态: 允许');
        console.log('如果要测试阻止功能，请在user_filter.txt中添加：deny <?php echo $clientIP; ?>');
    </script>
</body>
</html>
