CPPC = c++

CFLAG = -Wall -Wextra -Werror -std=c++98

INCLUDES = -I./includes -I.

SRCS = srcs/webserv.cpp \


OBJS = $(SRCS:.cpp=.o)

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