#endif

存储单个 server {} block 中的配置：
listen ip:port
root
index 文件数组
autoindex on/off
client_max_body_size
error_page CODE path
一个 server 下所有 location 的列表
//不包含运行时逻辑，纯数据结构。