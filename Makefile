CPPC = c++

CFLAG = -Wall -Wextra -Werror -std=c++98

INCLUDES = -I./includes -I.

SRCS =	HTTP/cpp/HTTPRequest.cpp \
		HTTP/cpp/HTTPRequestParser.cpp \
		HTTP/cpp/HTTPResponse.cpp \
		HTTP/cpp/ResponseBuilder.cpp \
		HTTP/cpp/ErrorResponse.cpp \
		HTTP/cpp/HTTPUtils.cpp \
		HTTP/cpp/RequestFactory.cpp \
		Method_Handle/cpp/DeleteRequest.cpp \
		Method_Handle/cpp/DirectoryHandle.cpp \
		Method_Handle/cpp/ErrorRequest.cpp \
		Method_Handle/cpp/FileUtils.cpp \
		Method_Handle/cpp/GetRequest.cpp \
		Method_Handle/cpp/PostRequest.cpp \
		Method_Handle/cpp/RedirectHandle.cpp \
		Method_Handle/cpp/StaticHandle.cpp \
		Method_Handle/cpp/UploadHandle.cpp \
 		Event/cpp/Client.cpp \
 		Event/cpp/EpollManager.cpp \
 		Event/cpp/Server.cpp \
 		CGI/cpp/CGIProcess.cpp \
		test_methods.cpp  
	#	test_http_parser.cpp  

	

OBJS = $(SRCS:.cpp=.o)

NAME = test_methods
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