tests:	check

CFLAGS=	-std=c99 -Wall -Werror

queue:	queue.c
	$(CC) $(CFLAGS) -o queue queue.c

dqueue:	dqueue.c
	$(CC) $(CFLAGS) -o dqueue dqueue.c

check:	queue dqueue
	./queue
	./dqueue

clean:
	rm -f queue dqueue
