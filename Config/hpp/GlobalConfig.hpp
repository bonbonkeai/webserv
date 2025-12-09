#endif

默认值 global defaults
由 ConfigParser 内部硬编码设置，可能并不需要独立类

可以直接在 ServerConfig 中设为默认值
ex:
default root
default autoindex
default client_max_body_size
default index 列表
default error_page

这些应该可以：
写在 ConfigParser 的构造函数里
或写在 ServerConfig 的默认构造函数里

！！所以这个文件或许可以不要