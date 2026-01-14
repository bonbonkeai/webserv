#endif

/*统一管理错误页：
找到对应的 error_page 文件 → 读入内容
如果没有定义，生成默认 HTML
返回一个 HTTPResponse 给 C/B 使用
避免所有 Request handler 都重复写错误页逻辑。*/