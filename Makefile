CC      = gcc
CFLAGS  = -Wall -Wextra

ifeq ($(OS),Windows_NT)
SOCKET_LIBS = -lws2_32
else
SOCKET_LIBS =
endif

all: tcp_sim node

tcp_sim: tcp_sim.c
	$(CC) $(CFLAGS) -o tcp_sim tcp_sim.c

node: node.c node.h
	$(CC) $(CFLAGS) -o node node.c $(SOCKET_LIBS)

clean:
	rm -f tcp_sim tcp_sim.exe node node.exe

.PHONY: all clean
