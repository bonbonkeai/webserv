#!/bin/sh



#printf "Content-Type: text/plain\r\n"
#sleep 1
#printf "Content-Length: 6\r\n"
#sleep 1
#printf "\r\n"
#printf "123456"


printf "Content-Type: text/plain\r\n"
printf "Content-Length: 4\r\n"
printf "\r\n"
printf "ABCD"
printf "EFGH"   # 多余输出


#printf "Content-Type: text/plain\r\n"
#printf "Content-Length: 5\r\n"
#printf "\r\n"
#printf "HELLO"
#sleep 2
