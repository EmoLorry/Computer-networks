<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>罗瑞的个人主页</title>
    <link rel="stylesheet" href="style.css">
    <link rel="icon" href="favicon.ico" type="image/x-icon">
</head>
<body>
    <div class="sidebar">
        <div class="logo">
            <img src="logo.jpg" alt="个人LOGO">
            <h2>罗瑞的主页</h2>
        </div>
        <nav>
            <ul>
                <li><a href="#info">关于我</a></li>
                <li><a href="#audio">自我介绍</a></li>
                <li><a href="#links">我的主页</a></li>
            </ul>
        </nav>
    </div>

    <div class="main-content">
        <header>
            <h1>欢迎来到我的个人主页</h1>
        </header>

        <section id="info" class="info">
            <h2>关于我</h2>
            <p><strong>姓名:</strong> 罗瑞</p>
            <p><strong>学号:</strong> 2210529</p>
            <p><strong>专业:</strong> 密码科学与技术</p>
        </section>

        <section id="audio" class="audio">
            <h2>自我介绍</h2>
            <audio controls>
                <source src="intro.mp3" type="audio/mp3">
                您的浏览器不支持音频播放。
            </audio>
        </section>

        <section id="links" class="links">
            <h2>我的主页</h2>
            <ul>
                <li><a href="https://github.com/EmoLorry" target="_blank">GitHub：个人代码仓库</a></li>
                <li><a href="http://47.76.44.83" target="_blank">个人博客：简单项目上线</a></li>
            </ul>
        </section>
    </div>
</body>
</html>
