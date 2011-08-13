OBJS=		main.o
R=		../../..
CFLAGS=		-Wall -Werror -std=c99 -D_GNU_SOURCE -g

config:		$(OBJS)
		cc -o config $(OBJS)

clean:
		rm -f config $(OBJS)
