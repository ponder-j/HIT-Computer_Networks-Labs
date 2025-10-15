<?php
// 设置Last-Modified
$lastModified = gmdate('D, d M Y H:i:s', time() - 3600) . ' GMT';

// 处理If-Modified-Since
if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])) {
    if ($_SERVER['HTTP_IF_MODIFIED_SINCE'] == $lastModified) {
        header('HTTP/1.1 304 Not Modified');
        header('Last-Modified: ' . $lastModified);
        exit;
    }
}

header('Last-Modified: ' . $lastModified);
header('Content-Type: text/html; charset=utf-8');

// 获取所有HTTP请求头
$headers = getallheaders();
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HTTP头信息查看器</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }
        .container {
            max-width: 1000px;
            margin: 0 auto;
            background: white;
            padding: 40px;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
        }
        h1 {
            color: #667eea;
            text-align: center;
            margin-bottom: 10px;
        }
        .subtitle {
            text-align: center;
            color: #666;
            margin-bottom: 30px;
        }
        .section {
            margin: 30px 0;
        }
        .section h2 {
            color: #667eea;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
            margin-bottom: 20px;
        }
        .headers-table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
            background: white;
        }
        .headers-table th {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 15px;
            text-align: left;
            font-weight: normal;
        }
        .headers-table td {
            padding: 12px 15px;
            border-bottom: 1px solid #eee;
        }
        .headers-table tr:hover {
            background: #f8f9fa;
        }
        .header-name {
            font-weight: bold;
            color: #667eea;
            width: 250px;
        }
        .header-value {
            font-family: 'Courier New', monospace;
            color: #333;
            word-break: break-all;
        }
        .highlight {
            background: #fff3cd;
            padding: 2px 6px;
            border-radius: 3px;
        }
        .cache-highlight {
            background: #d1ecf1;
            padding: 2px 6px;
            border-radius: 3px;
            color: #0c5460;
            font-weight: bold;
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
        .back-btn {
            display: inline-block;
            margin-top: 20px;
            padding: 12px 30px;
            background: #667eea;
            color: white;
            text-decoration: none;
            border-radius: 5px;
        }
        .back-btn:hover {
            background: #764ba2;
        }
        .refresh-btn {
            display: inline-block;
            margin-top: 20px;
            margin-left: 10px;
            padding: 12px 30px;
            background: #4CAF50;
            color: white;
            text-decoration: none;
            border-radius: 5px;
            cursor: pointer;
        }
        .refresh-btn:hover {
            background: #45a049;
        }
        code {
            background: #f4f4f4;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
            color: #d63384;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>📊 HTTP 头信息查看器</h1>
        <p class="subtitle">查看客户端请求和服务器响应的HTTP头信息</p>

        <?php if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])): ?>
        <div class="success-box">
            <h3>🎉 检测到缓存验证请求！</h3>
            <p>客户端（代理服务器）发送了 <code>If-Modified-Since</code> 头，正在验证缓存是否有效。</p>
            <p><strong>If-Modified-Since:</strong> <span class="cache-highlight"><?php echo $_SERVER['HTTP_IF_MODIFIED_SINCE']; ?></span></p>
            <p><strong>Last-Modified:</strong> <span class="cache-highlight"><?php echo $lastModified; ?></span></p>
            <?php if ($_SERVER['HTTP_IF_MODIFIED_SINCE'] == $lastModified): ?>
            <p style="color: #4CAF50; font-weight: bold;">✅ 匹配！服务器返回 304 Not Modified</p>
            <?php else: ?>
            <p style="color: #ff9800; font-weight: bold;">⚠️ 不匹配！服务器返回 200 OK 和新内容</p>
            <?php endif; ?>
        </div>
        <?php else: ?>
        <div class="info-box">
            <h3>ℹ️ 首次请求</h3>
            <p>这是一个新的请求，没有携带 <code>If-Modified-Since</code> 头。</p>
            <p>服务器会返回 <code>Last-Modified</code> 头，代理可以缓存此响应。</p>
        </div>
        <?php endif; ?>

        <div class="section">
            <h2>📥 客户端请求头</h2>
            <table class="headers-table">
                <thead>
                    <tr>
                        <th>头部名称</th>
                        <th>值</th>
                    </tr>
                </thead>
                <tbody>
                    <?php foreach ($headers as $name => $value): ?>
                    <tr>
                        <td class="header-name">
                            <?php
                            if ($name == 'If-Modified-Since') {
                                echo '<span class="cache-highlight">' . htmlspecialchars($name) . '</span>';
                            } else {
                                echo htmlspecialchars($name);
                            }
                            ?>
                        </td>
                        <td class="header-value">
                            <?php
                            if ($name == 'If-Modified-Since') {
                                echo '<span class="cache-highlight">' . htmlspecialchars($value) . '</span>';
                            } else {
                                echo htmlspecialchars($value);
                            }
                            ?>
                        </td>
                    </tr>
                    <?php endforeach; ?>
                </tbody>
            </table>
        </div>

        <div class="section">
            <h2>📤 服务器响应头</h2>
            <table class="headers-table">
                <thead>
                    <tr>
                        <th>头部名称</th>
                        <th>值</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td class="header-name">HTTP Status</td>
                        <td class="header-value">200 OK</td>
                    </tr>
                    <tr>
                        <td class="header-name"><span class="cache-highlight">Last-Modified</span></td>
                        <td class="header-value"><span class="cache-highlight"><?php echo $lastModified; ?></span></td>
                    </tr>
                    <tr>
                        <td class="header-name">Content-Type</td>
                        <td class="header-value">text/html; charset=utf-8</td>
                    </tr>
                    <tr>
                        <td class="header-name">Date</td>
                        <td class="header-value"><?php echo gmdate('D, d M Y H:i:s') . ' GMT'; ?></td>
                    </tr>
                </tbody>
            </table>
        </div>

        <div class="section">
            <h2>🔧 服务器环境变量</h2>
            <table class="headers-table">
                <thead>
                    <tr>
                        <th>变量名</th>
                        <th>值</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td class="header-name">REQUEST_METHOD</td>
                        <td class="header-value"><?php echo $_SERVER['REQUEST_METHOD']; ?></td>
                    </tr>
                    <tr>
                        <td class="header-name">REQUEST_URI</td>
                        <td class="header-value"><?php echo $_SERVER['REQUEST_URI']; ?></td>
                    </tr>
                    <tr>
                        <td class="header-name">SERVER_PROTOCOL</td>
                        <td class="header-value"><?php echo $_SERVER['SERVER_PROTOCOL']; ?></td>
                    </tr>
                    <tr>
                        <td class="header-name">REMOTE_ADDR</td>
                        <td class="header-value"><?php echo $_SERVER['REMOTE_ADDR']; ?></td>
                    </tr>
                    <tr>
                        <td class="header-name">SERVER_SOFTWARE</td>
                        <td class="header-value"><?php echo $_SERVER['SERVER_SOFTWARE']; ?></td>
                    </tr>
                </tbody>
            </table>
        </div>

        <div class="info-box">
            <h3>💡 使用说明</h3>
            <ul>
                <li>首次访问此页面，记录 <code>Last-Modified</code> 的值</li>
                <li>刷新页面，查看是否出现 <code>If-Modified-Since</code> 头</li>
                <li>如果出现且值与 <code>Last-Modified</code> 匹配，说明代理正在验证缓存</li>
                <li>查看代理服务器日志，确认是否返回304响应</li>
            </ul>
        </div>

        <a href="index.html" class="back-btn">← 返回首页</a>
        <a href="headers.php" class="refresh-btn" onclick="location.reload(); return false;">🔄 刷新页面</a>
    </div>
</body>
</html>
