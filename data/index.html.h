#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<html>

<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>地址设置</title>
</head>

<style>
    /* 1. 默认隐藏所有内容块 */
    .content > div {
        display: none;
        padding: 10px;
        border: 1px solid #ddd;
        margin-top: 10px;
    }
    
    /* 2. 默认 Tab 样式 */
    .tabbox ul li {
        display: inline-block;
        padding: 8px 15px;
        cursor: pointer;
        background-color: #f0f0f0;
    }
    
    /* 3. 激活状态的样式 */
    .tabbox ul li.active {
        background-color: #007bff;
        color: #fff;
    }
    .content > div.active {
        display: block; /* 只有带有 active 的内容才会显示 */
    }
</style>

<body>
<div class="tabbox">
    <ul>
        <li class="active" data="address">地址设置</li>
        <li data="alarm">报警阀值</li>
        <li data="wifi">无线设置</li>
        <li data="ble">蓝牙设置</li>
        <li data="network">网络设置</li>
        <li data="service">服务设置</li>
    </ul>
    <div class="content">
        <div class="active" data="address">地址设置内容</div>
        <div data="alarm">报警阀值内容</div>
        <div data="wifi">无线设置内容</div>
        <div data="ble">蓝牙设置内容</div>
        <div data="network">网络设置内容</div>
        <div data="service">服务设置内容</div>
    </div>
</div>

<script>
    // 获取所有的 li 和 内容 div
    const tabs = document.querySelectorAll('.tabbox ul li');
    const contents = document.querySelectorAll('.content > div');

    // 使用事件委托，将点击事件绑定在父元素 ul 上
    document.querySelector('.tabbox ul').addEventListener('click', function(event) {
        const target = event.target;
        
        // 确保点击的是 li 元素
        if (target.tagName === 'LI') {
            // 1. 获取当前点击的 li 的 data 属性值
            const currentData = target.getAttribute('data');
            
            // 2. 移除所有 li 的 active 状态
            tabs.forEach(tab => tab.classList.remove('active'));
            
            // 3. 核心：移除所有内容的 active 状态（实现其他内容隐藏）
            contents.forEach(content => content.classList.remove('active'));
            
            // 4. 给当前点击的 li 添加 active 状态
            target.classList.add('active');
            
            // 5. 根据 data 属性，找到对应的内容块并添加 active 状态（实现对应内容显示）
            const targetContent = document.querySelector(`.content > div[data="${currentData}"]`);
            if (targetContent) {
                targetContent.classList.add('active');
            }
            // 6. 更新页面标题为当前点击的 li 文本内容
            document.title = target.textContent;
        }
    });
</script>
</body>

</html>
)rawliteral";

#endif // INDEX_HTML_H