<?php
// 设置Last-Modified为固定时间
date_default_timezone_set('Asia/Shanghai');
$lastModified = gmdate('D, d M Y H:i:s', strtotime('2025-01-15 12:00:00')) . ' GMT';

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
$pageGenNumber = rand(10000, 99999);
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>缓存功能测试</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #4CAF50 0%, #8BC34A 100%);
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
            color: #4CAF50;
            text-align: center;
            margin-bottom: 30px;
        }

        .time-display {
            background: linear-gradient(135deg, #4CAF50 0%, #8BC34A 100%);
            color: white;
            padding: 40px;
            border-radius: 15px;
            text-align: center;
            margin-bottom: 30px;
        }

        .time-display h2 {
            font-size: 3em;
            margin: 0;
            font-family: 'Courier New', monospace;
        }

        .time-display p {
            margin: 10px 0 0 0;
            opacity: 0.9;
            font-size: 1.2em;
        }

        .page-id {
            text-align: center;
            font-size: 1.5em;
            color: #666;
            margin: 20px 0;
        }

        .info-box {
            background: #e8f5e9;
            border-left: 4px solid #4CAF50;
            padding: 20px;
            margin: 20px 0;
            border-radius: 5px;
        }

        .info-box h3 {
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

        .http-info {
            background: #f5f5f5;
            padding: 20px;
            border-radius: 10px;
            margin: 20px 0;
        }

        .http-info table {
            width: 100%;
            border-collapse: collapse;
        }

        .http-info th,
        .http-info td {
            padding: 10px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }

        .http-info th {
            background: white;
            font-weight: bold;
            width: 40%;
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

        .refresh-btn {
            display: inline-block;
            margin: 10px 5px;
            padding: 12px 25px;
            background: #4CAF50;
            color: white;
            text-decoration: none;
            border-radius: 20px;
            cursor: pointer;
            border: none;
            font-size: 1em;
            font-weight: bold;
        }

        .refresh-btn:hover {
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
        <h1>💾 HTTP缓存功能测试</h1>

        <div class="time-display">
            <h2><?php echo $generatedTime; ?></h2>
            <p>页面生成时间</p>
        </div>

        <div class="page-id">
            页面ID: <strong>#<?php echo $pageGenNumber; ?></strong>
        </div>

        <?php if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])): ?>
            <div class="info-box">
                <h3>✅ 检测到缓存验证请求</h3>
                <p>代理服务器发送了 <code>If-Modified-Since</code> 头。</p>
                <p><strong>If-Modified-Since:</strong> <?php echo $_SERVER['HTTP_IF_MODIFIED_SINCE']; ?></p>
                <?php if ($_SERVER['HTTP_IF_MODIFIED_SINCE'] == $lastModified): ?>
                    <p style="color: #4CAF50; font-weight: bold;">✓ 匹配成功！服务器返回 304 Not Modified</p>
                    <p>如果时间和页面ID保持不变，说明代理正确使用了缓存。</p>
                <?php endif; ?>
            </div>
        <?php else: ?>
            <div class="warning-box">
                <h3>ℹ️ 首次请求</h3>
                <p>这是一个新的请求，没有携带 <code>If-Modified-Since</code> 头。</p>
                <p>服务器会返回 <code>Last-Modified</code> 头，代理可以缓存此响应。</p>
            </div>
        <?php endif; ?>

        <h2 style="color: #4CAF50; margin-top: 30px;">🧪 如何测试</h2>
        <div class="info-box" style="background: #e3f2fd; border-left-color: #2196F3;">
            <h3 style="color: #1976d2;">测试步骤：</h3>
            <ol>
                <li><strong>记录当前时间：</strong><?php echo $generatedTime; ?></li>
                <li><strong>刷新页面（F5）：</strong>多次刷新</li>
                <li><strong>观察时间变化：</strong>
                    <ul style="margin-top: 10px;">
                        <li>✅ <strong>时间不变</strong> → 缓存工作正常</li>
                        <li>❌ <strong>时间改变</strong> → 缓存未生效</li>
                    </ul>
                </li>
                <li><strong>查看页面ID：</strong>应该保持为 <strong>#<?php echo $pageGenNumber; ?></strong></li>
            </ol>
        </div>

        <h2 style="color: #4CAF50; margin-top: 30px;">📊 HTTP头信息</h2>
        <div class="http-info">
            <table>
                <tr>
                    <th>Last-Modified</th>
                    <td><code><?php echo $lastModified; ?></code></td>
                </tr>
                <tr>
                    <th>If-Modified-Since</th>
                    <td><code><?php echo isset($_SERVER['HTTP_IF_MODIFIED_SINCE']) ? $_SERVER['HTTP_IF_MODIFIED_SINCE'] : '(未发送)'; ?></code></td>
                </tr>
                <tr>
                    <th>Content-Type</th>
                    <td><code>text/html; charset=utf-8</code></td>
                </tr>
                <tr>
                    <th>请求方法</th>
                    <td><code><?php echo $_SERVER['REQUEST_METHOD']; ?></code></td>
                </tr>
            </table>
        </div>

        <h2 style="color: #4CAF50; margin-top: 30px;">📝 代理日志参考</h2>
        <div style="background: #f5f5f5; padding: 20px; border-radius: 10px;">
            <p><strong>首次请求：</strong></p>
            <pre style="background: white; padding: 15px; border-radius: 5px; overflow-x: auto; margin-top: 10px;">[缓存] 未找到缓存
[响应] 状态码: 200
[响应] Last-Modified: <?php echo $lastModified; ?>

[缓存] 已保存缓存，大小: xxx 字节</pre>

            <p style="margin-top: 15px;"><strong>再次请求：</strong></p>
            <pre style="background: white; padding: 15px; border-radius: 5px; overflow-x: auto; margin-top: 10px;">[缓存] 找到缓存，大小: xxx 字节
[缓存] 添加 If-Modified-Since: <?php echo $lastModified; ?>

[响应] 状态码: 304
[缓存] 服务器返回304，使用缓存替代响应
[缓存] 成功发送缓存数据到客户端</pre>
        </div>

        <div class="btn-container">
            <button class="refresh-btn" onclick="location.reload()">🔄 刷新测试</button>
            <a href="no-cache-test.php" class="refresh-btn" style="background: #ff9800; display: inline-block;">测试无缓存页面</a>
            <a href="index.html" class="back-btn">← 返回测试中心</a>
        </div>

        <p style="margin-top: 30px; text-align: center; color: #999; font-size: 0.9em;">
            <strong>页面生成时间：</strong><?php echo $generatedTime; ?><br>
            <strong>页面ID：</strong>#<?php echo $pageGenNumber; ?>
        </p>
    </div>

    <script>
        console.log('=== 缓存功能测试 ===');
        console.log('页面生成时间:', '<?php echo $generatedTime; ?>');
        console.log('页面ID:', '#<?php echo $pageGenNumber; ?>');
        console.log('Last-Modified:', '<?php echo $lastModified; ?>');
        <?php if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])): ?>
        console.log('If-Modified-Since:', '<?php echo $_SERVER['HTTP_IF_MODIFIED_SINCE']; ?>');
        console.log('缓存验证: 已发送');
        <?php else: ?>
        console.log('If-Modified-Since: 未发送（首次请求）');
        <?php endif; ?>
        console.log('提示: 刷新页面查看时间是否改变');
    </script>
</body>
</html>
