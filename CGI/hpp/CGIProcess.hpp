#endif

完整 CGI 子进程管理：
建立 pipe
fork
子进程 dup2 → execve
父进程写 body 到 stdin
父进程将 stdout fd 加入 B 的 poll
解析完毕后返回到 Request handle。