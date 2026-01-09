CPPC = c++

CFLAG = -Wall -Wextra -Werror -std=c++98

INCLUDES = -I./includes -I.

SRCS =	main.cpp  \
		HTTP/cpp/HTTPRequest.cpp \
		HTTP/cpp/HTTPRequestParser.cpp \
		HTTP/cpp/HTTPResponse.cpp \
		HTTP/cpp/ResponseBuilder.cpp \
		HTTP/cpp/ErrorResponse.cpp \
		HTTP/cpp/HTTPUtils.cpp \
		Event/cpp/Server.cpp\
		Event/cpp/EpollManager.cpp\
		Event/cpp/Client.cpp\
		CGI/cpp/CGIProcess.cpp

	

OBJS = $(SRCS:.cpp=.o)

NAME = test_http
# NAME = webserv

all: $(NAME)

$(NAME): $(OBJS)
	$(CPPC) $(CFLAG) $(OBJS) -o $(NAME)

%.o:%.cpp
	$(CPPC) $(INCLUDES) $(CFLAG) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

debug: CFLAG += -DDEBUG_SHOW -g
debug: fclean all

.PHONY: all clean fclean re