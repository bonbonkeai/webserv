#endif

配置层的核心逻辑：
根据 client 请求的 Host header 选择正确的 ServerConfig
根据 URI longest-prefix-match 匹配 LocationConfig
合并 server + location 得到最终有效配置对象 EffectiveConfig
//拼接
服务器所有行为 (静态文件根目录、cgi、index…) 都依赖这个组件