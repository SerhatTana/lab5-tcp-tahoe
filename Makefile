CC      = gcc
CFLAGS  = -Wall -Wextra -o

all: tcp_sim node

tcp_sim: tcp_sim.c
	$(CC) $(CFLAGS) tcp_sim tcp_sim.c

node: node.c
	$(CC) $(CFLAGS) node node.c

clean:
	rm -f tcp_sim node

.PHONY: all clean
