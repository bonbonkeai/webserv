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
		HTTP/cpp/Session.cpp \
		Config/cpp/ConfigTokenizer.cpp \
		Config/cpp/ConfigParser.cpp \
		Config/cpp/ConfigUtils.cpp \
		Config/cpp/EffectiveConfig.cpp \
		Config/cpp/Routing.cpp \
		Config/cpp/ErrorPage.cpp \
		Method_Handle/cpp/DeleteRequest.cpp \
		Method_Handle/cpp/DirectoryHandle.cpp \
		Method_Handle/cpp/ErrorRequest.cpp \
		Method_Handle/cpp/FileUtils.cpp \
		Method_Handle/cpp/GetRequest.cpp \
		Method_Handle/cpp/PostRequest.cpp \
		Method_Handle/cpp/RedirectHandle.cpp \
		Method_Handle/cpp/StaticHandle.cpp \
		Method_Handle/cpp/UploadHandle.cpp \
		Method_Handle/cpp/CGIRequestHandle.cpp \
 		Event/cpp/Client.cpp \
 		Event/cpp/EpollManager.cpp \
 		Event/cpp/Server.cpp \
 		CGI/cpp/CGIProcess.cpp \
		CGI/cpp/CGIManager.cpp \
		webserv.cpp
# 		main.cpp

# 		test_http.cpp 
# 		test_http_parser.cpp 
# 		test_methods.cpp  
	

OBJS = $(SRCS:.cpp=.o)

# NAME = test_http
NAME = webserv

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
