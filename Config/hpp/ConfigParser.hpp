#endif


/*读取配置文件

词法分析 tokenize
语法分析（解析 server { ... } 结构）
构造：
ServerConfig
LocationConfig
检查语法错误、缺失字段

最后得到std::vector<ServerConfig> servers;*/