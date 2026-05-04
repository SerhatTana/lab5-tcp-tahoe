#ifndef NODE_H
#define NODE_H

/*
 * node.h -- Shared definitions for the 6-node UDP routing program
 *           with TCP Tahoe congestion-control simulation.
 *
 * Compile:
 *   Windows: gcc -Wall -Wextra -o node node.c -lws2_32
 *   Linux  : gcc -Wall -Wextra -o node node.c
 *
 * Test:
 *   ./node A.conf
 *   ./node B.conf
 *   ./node C.conf
 *   ./node D.conf
 *   ./node E.conf
 *   ./node F.conf
 */

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  typedef int socklen_t;
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NODES       6
#define INF             9999999
#define BUF_SIZE        1024

#define MSS             1.0
#define INIT_CWND       1.0
#define INIT_SSTHRESH   16.0
#define TAHOE_MAX_RTT   12

typedef struct {
    int dist;
    int next_hop;
} Route;

typedef struct {
    char id;
    int idx;
    int port;
    int neighbor_port[MAX_NODES];
} NodeConf;

typedef enum {
    EVENT_ACK,
    EVENT_TIMEOUT,
    EVENT_TRIPLE_DUP_ACK
} PacketEventType;

typedef struct {
    int rtt;
    PacketEventType type;
} PacketEvent;

typedef struct {
    double cwnd;
    double ssthresh;
    int ack_counter;
} TahoeSenderState;

#endif
