<?php
// 设置Last-Modified
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

$imageGenTime = date('Y-m-d H:i:s');
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>图片缓存测试</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 900px;
            margin: 30px auto;
            padding: 20px;
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
        }
        .container {
            background: white;
            padding: 40px;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
        }
        h1 {
            color: #f5576c;
            text-align: center;
        }
        .image-container {
            text-align: center;
            margin: 30px 0;
            padding: 20px;
            background: #f8f9fa;
            border-radius: 10px;
        }
        .placeholder-image {
            width: 400px;
            height: 300px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-size: 2em;
            font-weight: bold;
            margin: 20px auto;
            border-radius: 10px;
            box-shadow: 0 5px 20px rgba(0,0,0,0.2);
        }
        .info-box {
            background: #e3f2fd;
            padding: 20px;
            border-left: 4px solid #2196F3;
            margin: 20px 0;
            border-radius: 5px;
        }
        .time-stamp {
            text-align: center;
            font-size: 1.5em;
            color: #f5576c;
            padding: 15px;
            background: #fff3f3;
            border-radius: 5px;
            margin: 20px 0;
            font-family: 'Courier New', monospace;
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
            background: #f5576c;
            color: white;
        }
        .back-btn {
            display: inline-block;
            margin-top: 20px;
            padding: 12px 30px;
            background: #f5576c;
            color: white;
            text-decoration: none;
            border-radius: 5px;
        }
        .back-btn:hover {
            background: #f093fb;
        }
        code {
            background: #f4f4f4;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>📸 图片资源缓存测试</h1>

        <div class="time-stamp">
            页面生成时间: <?php echo $imageGenTime; ?>
        </div>

        <div class="image-container">
            <h3>模拟图片资源</h3>
            <div class="placeholder-image">
                🖼️<br>TEST IMAGE
            </div>
            <p style="color: #666;">（这是一个模拟的图片，用于测试图片资源的缓存）</p>
        </div>

        <div class="info-box">
            <h3>🎯 测试说明</h3>
            <p>虽然这是一个HTML页面而不是真实的图片，但它模拟了图片资源的缓存行为：</p>
            <ul>
                <li>✅ 发送 <code>Last-Modified</code> 头</li>
                <li>✅ 响应 <code>If-Modified-Since</code> 验证</li>
                <li>✅ 返回 304 Not Modified（如果未修改）</li>
            </ul>
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
                <td><strong>Content-Type</strong></td>
                <td><code>text/html</code></td>
            </tr>
        </table>

        <?php if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])): ?>
        <div class="info-box" style="background: #e8f5e9; border-left-color: #4CAF50;">
            <h3>✅ 缓存验证请求</h3>
            <p>代理服务器发送了 <code>If-Modified-Since</code> 头。</p>
            <?php if ($_SERVER['HTTP_IF_MODIFIED_SINCE'] == $lastModified): ?>
            <p><strong style="color: #4CAF50;">匹配成功！服务器返回 304 Not Modified</strong></p>
            <p>如果页面生成时间保持不变，说明代理正确使用了缓存。</p>
            <?php endif; ?>
        </div>
        <?php endif; ?>

        <div class="info-box">
            <h3>📝 真实图片缓存</h3>
            <p>在实际应用中，图片文件（如PNG、JPG、GIF）会被Nginx自动添加Last-Modified头（基于文件修改时间）。</p>
            <p>代理服务器的缓存机制对所有包含Last-Modified头的资源都有效，包括：</p>
            <ul>
                <li>静态HTML文件</li>
                <li>图片文件（PNG、JPG、GIF等）</li>
                <li>CSS样式表</li>
                <li>JavaScript文件</li>
                <li>任何动态生成但包含Last-Modified的内容</li>
            </ul>
        </div>

        <a href="index.html" class="back-btn">← 返回首页</a>
    </div>
</body>
</html>
