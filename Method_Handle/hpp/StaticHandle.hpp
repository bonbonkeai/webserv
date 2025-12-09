#endif

打开文件
读入内容
根据扩展名确定 MIME 类型：
[当浏览器访问一个静态文件->ex: GET /images/cat.png
服务器必须返回：
Content-Type: image/png
这样浏览器才知道它收到的是PNG图片
而不是纯文本或别的格式。
Donc:
根据扩展名（.html / .css / .png / .jpg / .js / .json / .mp4）
查表决定MIME类型（text/html, image/png, ...）。]

填充 HTTPResponse