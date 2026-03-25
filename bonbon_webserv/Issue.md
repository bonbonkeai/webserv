# Issues

## 1.网站写的跟你们的实际行为不符，很多应该返回200的返回了404之类的 


## 2.使用valgrind 打开 webserv， siege 压力测试的时候，执行CGI会有内存错误
  似乎有几种内存错误，都是在压力测试的时候执行CGI产生的

==21095== Warning: invalid file descriptor 82761808 in syscall close()
==21095== Invalid write of size 4
==21095==    at 0x153DE2: CGI_Process::terminate() (CGIProcess.cpp:190)
==21095==    by 0x154CE9: CGIManager::kill_and_remove(CGI_Process*) (CGIManager.cpp:124)
==21095==    by 0x14BAD0: Server::handle_cgi_event(int, unsigned int) (new_Server.cpp:635)
==21095==    by 0x14CA05: Server::run_handle_event(int, unsigned int) (new_Server.cpp:854)
==21095==    by 0x14CB41: Server::run() (new_Server.cpp:893)
==21095==    by 0x156FBC: main (webserv.cpp:31)

==21095== Invalid free() / delete / delete[] / realloc()
==21095==    at 0x484B8AF: operator delete(void*) (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==21095==    by 0x154D15: CGIManager::kill_and_remove(CGI_Process*) (CGIManager.cpp:126)
==21095==    by 0x14BAD0: Server::handle_cgi_event(int, unsigned int) (new_Server.cpp:635)
==21095==    by 0x14CA05: Server::run_handle_event(int, unsigned int) (new_Server.cpp:854)
==21095==    by 0x14CB41: Server::run() (new_Server.cpp:893)
==21095==    by 0x156FBC: main (webserv.cpp:31)
==21095==  Address 0x4e3c0f0 is 11 bytes after a block of size 21 free'd


## 3.我配置了 listen 127.0.0.1:9080 但是你们监听的是0.0.0.0， 这两个不应该一样，应该是配置什么IP就监听什么IP

webserv booting
config: config_defaut/default.conf
server blocks: 3
listening on:
  - 0.0.0.0:8080
  - 0.0.0.0:8090
  - 0.0.0.0:9080

0.0.0.0 代表监听任何IP, 127.0.0.1监听本地IP, webserv 应该要可以配置任何IP

## 4.用我的网站的时候的问题

1 - 上传文件可以，但有时候无法删除
2 - CGI无法上传文件 似乎
3 - Python CGI 可以执行，php 无法执行，可能因为你们不支持php， 没关系
4 - POST的时候，content-type 重复了没有报错，不是最重要的，可以不处理

    "POST, repeat content length., return 400" \
    "POST /upload HTTP/1.1\r\nHost: localhost\r
    Content-Length: 0\r
    Content-Length: 1\r
    \r\n\r" \
    "400"
