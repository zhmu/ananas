OBJS=		main.o
R=		../../..
CXXFLAGS=	-Wall -Werror -std=c++11 -D_GNU_SOURCE -g

config:		$(OBJS)
		$(CXX) -o config $(OBJS)

clean:
		rm -f config $(OBJS)
