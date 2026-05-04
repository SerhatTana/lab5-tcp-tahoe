/*
 * node.c -- 6-node UDP routing with TCP Tahoe simulation
 *
 * CSE 320 Computer Networks Assignment
 *
 * The program is started in six different terminals:
 *   ./node A.conf
 *   ./node B.conf
 *   ./node C.conf
 *   ./node D.conf
 *   ./node E.conf
 *   ./node F.conf
 *
 * A user command such as:
 *   send D hello_from_A_to_D
 *
 * first prints TCP Tahoe cwnd evolution on the sender, then forwards
 * the message hop-by-hop over UDP according to the Dijkstra routing table.
 */

#include "node.h"

static const char NODE_NAMES[] = "ABCDEF";

/*
 * Static assignment topology. Every node knows the same link-state map
 * and runs Dijkstra locally.
 *
 * A-D direct cost is 13, while A-B-D cost is 4 + 8 = 12.
 * Therefore, A must choose B as next hop for destination D.
 */
static const int TOPO[MAX_NODES][MAX_NODES] = {
    /* A    B    C    D    E    F */
    {  0,   4,   7,  13, INF,   5 }, /* A */
    {  4,   0, INF,   8,   3, INF }, /* B */
    {  7, INF,   0,   9,  12, INF }, /* C */
    { 13,   8,   9,   0, INF, INF }, /* D */
    { INF,  3,  12, INF,   0, INF }, /* E */
    {  5, INF, INF, INF, INF,   0 }  /* F */
};

/*
 * Configurable Tahoe loss pattern. Edit this table to demonstrate
 * different timeout or triple-duplicate-ACK scenarios.
 */
static const PacketEvent TAHOE_EVENTS[] = {
    { 4, EVENT_TIMEOUT },
    { 8, EVENT_TRIPLE_DUP_ACK }
};

static const size_t TAHOE_EVENT_COUNT =
    sizeof(TAHOE_EVENTS) / sizeof(TAHOE_EVENTS[0]);

static Route routing_table[MAX_NODES];
static NodeConf self_node;

static int node_index(char id)
{
    const char *p = strchr(NODE_NAMES, id);
    if (p == NULL) {
        return -1;
    }
    return (int)(p - NODE_NAMES);
}

static PacketEventType packet_event_at_rtt(int rtt)
{
    size_t i;

    for (i = 0; i < TAHOE_EVENT_COUNT; i++) {
        if (TAHOE_EVENTS[i].rtt == rtt) {
            return TAHOE_EVENTS[i].type;
        }
    }

    return EVENT_ACK;
}

static void print_tahoe_state(int rtt,
                              const TahoeSenderState *state,
                              const char *event)
{
    printf("[TCP Tahoe] RTT %2d | cwnd=%5.1f MSS | ssthresh=%5.1f | %s\n",
           rtt, state->cwnd, state->ssthresh, event);
}

static void tahoe_on_ack(TahoeSenderState *state)
{
    if (state->cwnd < state->ssthresh) {
        state->cwnd *= 2.0;
        if (state->cwnd > state->ssthresh) {
            state->cwnd = state->ssthresh;
        }
    } else {
        state->cwnd += MSS;
    }

    state->ack_counter++;
}

static void tahoe_on_loss(TahoeSenderState *state)
{
    state->ssthresh = state->cwnd / 2.0;
    if (state->ssthresh < MSS) {
        state->ssthresh = MSS;
    }

    state->cwnd = MSS;
    state->ack_counter = 0;
}

static void simulate_tcp_tahoe(char src, char dst)
{
    TahoeSenderState state;
    int rtt;

    state.cwnd = INIT_CWND;
    state.ssthresh = INIT_SSTHRESH;
    state.ack_counter = 0;

    printf("\n[%c] TCP Tahoe simulation before sending to %c\n", src, dst);
    print_tahoe_state(0, &state, "Initial sender state");

    for (rtt = 1; rtt <= TAHOE_MAX_RTT; rtt++) {
        PacketEventType event = packet_event_at_rtt(rtt);

        if (event == EVENT_TIMEOUT) {
            tahoe_on_loss(&state);
            print_tahoe_state(rtt, &state,
                              "TIMEOUT / packet loss -> ssthresh=cwnd/2, cwnd=1");
        } else if (event == EVENT_TRIPLE_DUP_ACK) {
            tahoe_on_loss(&state);
            print_tahoe_state(rtt, &state,
                              "3 DUP ACK / Fast Retransmit -> ssthresh=cwnd/2, cwnd=1 (Tahoe)");
        } else {
            const char *phase =
                (state.cwnd < state.ssthresh) ? "Slow Start" : "Congestion Avoidance";
            tahoe_on_ack(&state);
            print_tahoe_state(rtt, &state, phase);
        }
    }

    printf("[%c] TCP Tahoe simulation finished. Message transmission starts.\n\n", src);
}

static void dijkstra(int src)
{
    int visited[MAX_NODES] = {0};
    int i;
    int iter;

    for (i = 0; i < MAX_NODES; i++) {
        routing_table[i].dist = INF;
        routing_table[i].next_hop = -1;
    }

    routing_table[src].dist = 0;
    routing_table[src].next_hop = src;

    for (i = 0; i < MAX_NODES; i++) {
        if (i != src && TOPO[src][i] < INF) {
            routing_table[i].dist = TOPO[src][i];
            routing_table[i].next_hop = i;
        }
    }

    for (iter = 0; iter < MAX_NODES - 1; iter++) {
        int u = -1;

        for (i = 0; i < MAX_NODES; i++) {
            if (!visited[i] && routing_table[i].dist < INF) {
                if (u == -1 || routing_table[i].dist < routing_table[u].dist) {
                    u = i;
                }
            }
        }

        if (u == -1) {
            break;
        }

        visited[u] = 1;

        for (i = 0; i < MAX_NODES; i++) {
            if (!visited[i] && TOPO[u][i] < INF) {
                int alt = routing_table[u].dist + TOPO[u][i];

                if (alt < routing_table[i].dist) {
                    routing_table[i].dist = alt;
                    routing_table[i].next_hop = routing_table[u].next_hop;
                }
            }
        }
    }
}

static void print_routing_table(void)
{
    int i;

    printf("\n[%c] Routing Table (Dijkstra)\n", self_node.id);
    printf("%-12s %-10s %-6s\n", "Destination", "Next Hop", "Cost");
    printf("-----------------------------\n");

    for (i = 0; i < MAX_NODES; i++) {
        char dest = NODE_NAMES[i];
        char next = routing_table[i].next_hop >= 0
                    ? NODE_NAMES[routing_table[i].next_hop]
                    : '-';

        if (routing_table[i].dist >= INF) {
            printf("%-12c %-10c %-6s\n", dest, next, "INF");
        } else {
            printf("%-12c %-10c %-6d\n",
                   dest, next, routing_table[i].dist);
        }
    }

    printf("\n");
}

static int parse_conf(const char *filename)
{
    FILE *file = fopen(filename, "r");
    char key[32];

    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    memset(&self_node, 0, sizeof(self_node));
    self_node.idx = -1;

    while (fscanf(file, "%31s", key) == 1) {
        if (strcmp(key, "node_id") == 0) {
            char value[8];
            if (fscanf(file, "%7s", value) != 1) {
                fclose(file);
                return -1;
            }
            self_node.id = value[0];
            self_node.idx = node_index(self_node.id);
        } else if (strcmp(key, "port") == 0) {
            if (fscanf(file, "%d", &self_node.port) != 1) {
                fclose(file);
                return -1;
            }
        } else if (strcmp(key, "neighbor") == 0) {
            char neighbor_id[8];
            int neighbor_port;
            int neighbor_cost;
            int idx;

            if (fscanf(file, "%7s %d %d",
                       neighbor_id, &neighbor_port, &neighbor_cost) != 3) {
                fclose(file);
                return -1;
            }

            (void)neighbor_cost;
            idx = node_index(neighbor_id[0]);
            if (idx >= 0) {
                self_node.neighbor_port[idx] = neighbor_port;
            }
        }
    }

    fclose(file);

    if (self_node.idx < 0 || self_node.port <= 0) {
        fprintf(stderr, "Invalid config file: %s\n", filename);
        return -1;
    }

    return 0;
}

static int send_udp(int dest_port, const char *msg)
{
    struct sockaddr_in addr;

#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket error: %d\n", WSAGetLastError());
        return -1;
    }
#else
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
#endif

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)dest_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (sendto(sock, msg, (int)strlen(msg), 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        fprintf(stderr, "sendto error: %d\n", WSAGetLastError());
        closesocket(sock);
#else
        perror("sendto");
        close(sock);
#endif
        return -1;
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    return 0;
}

/*
 * Packet format:
 *   SRC DST BODY
 *
 * Example:
 *   A D hello_from_A_to_D
 */
static void forward_message(const char *raw)
{
    char src_text[8];
    char dst_text[8];
    char body[BUF_SIZE];
    int dst_idx;
    int next_hop;
    int next_port;
    char packet[BUF_SIZE + 32];

    if (sscanf(raw, "%7s %7s %1023[^\n]", src_text, dst_text, body) != 3) {
        return;
    }

    dst_idx = node_index(dst_text[0]);
    if (dst_idx < 0) {
        printf("[%c] Invalid destination: %s\n", self_node.id, dst_text);
        return;
    }

    if (dst_idx == self_node.idx) {
        printf("[%c] Received message from %s: %s\n",
               self_node.id, src_text, body);
        return;
    }

    next_hop = routing_table[dst_idx].next_hop;
    if (next_hop < 0 || next_hop == self_node.idx) {
        printf("[%c] No route to %s\n", self_node.id, dst_text);
        return;
    }

    next_port = self_node.neighbor_port[next_hop];
    if (next_port == 0) {
        printf("[%c] Next hop %c is not a configured neighbor\n",
               self_node.id, NODE_NAMES[next_hop]);
        return;
    }

    if (self_node.id == src_text[0]) {
        printf("[%c] Destination %s, next hop %c (cost %d)\n",
               self_node.id, dst_text, NODE_NAMES[next_hop],
               routing_table[dst_idx].dist);
    } else {
        printf("[%c] Forwarding message from %s to %s, next hop %c\n",
               self_node.id, src_text, dst_text, NODE_NAMES[next_hop]);
    }

    snprintf(packet, sizeof(packet), "%s %s %s", src_text, dst_text, body);
    send_udp(next_port, packet);
}

#ifdef _WIN32
#define QUEUE_SIZE 32
static char cmd_queue[QUEUE_SIZE][BUF_SIZE];
static int q_head = 0;
static int q_tail = 0;
static CRITICAL_SECTION q_lock;
static volatile int g_quit = 0;

static int q_push(const char *line)
{
    int next;

    EnterCriticalSection(&q_lock);
    next = (q_tail + 1) % QUEUE_SIZE;
    if (next == q_head) {
        LeaveCriticalSection(&q_lock);
        return -1;
    }

    strncpy(cmd_queue[q_tail], line, BUF_SIZE - 1);
    cmd_queue[q_tail][BUF_SIZE - 1] = '\0';
    q_tail = next;
    LeaveCriticalSection(&q_lock);
    return 0;
}

static int q_pop(char *out)
{
    EnterCriticalSection(&q_lock);
    if (q_head == q_tail) {
        LeaveCriticalSection(&q_lock);
        return -1;
    }

    strncpy(out, cmd_queue[q_head], BUF_SIZE - 1);
    out[BUF_SIZE - 1] = '\0';
    q_head = (q_head + 1) % QUEUE_SIZE;
    LeaveCriticalSection(&q_lock);
    return 0;
}

static DWORD WINAPI stdin_thread(LPVOID arg)
{
    char line[BUF_SIZE];
    (void)arg;

    while (!g_quit) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        line[strcspn(line, "\r\n")] = '\0';
        q_push(line);

        if (strcmp(line, "quit") == 0) {
            break;
        }
    }

    return 0;
}
#endif

static void handle_command(const char *line)
{
    char cmd[16];
    char dst[8];
    char msg[BUF_SIZE];
    char packet[BUF_SIZE + 32];
    int dst_idx;

    if (sscanf(line, "%15s %7s %1023[^\n]", cmd, dst, msg) != 3) {
        printf("[%c] Command format: send <DST> <message>\n", self_node.id);
        return;
    }

    if (strcmp(cmd, "send") != 0) {
        printf("[%c] Unknown command: %s\n", self_node.id, cmd);
        return;
    }

    dst_idx = node_index(dst[0]);
    if (dst_idx < 0) {
        printf("[%c] Invalid destination: %s\n", self_node.id, dst);
        return;
    }

    simulate_tcp_tahoe(self_node.id, dst[0]);
    snprintf(packet, sizeof(packet), "%c %c %s", self_node.id, dst[0], msg);
    forward_message(packet);
}

static void run_node(void)
{
    char line[BUF_SIZE];
    char udp_buf[BUF_SIZE];
    struct sockaddr_in local_addr;

#ifdef _WIN32
    WSADATA wsa;
    SOCKET udp_sock;
    HANDLE input_thread;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return;
    }

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == INVALID_SOCKET) {
        fprintf(stderr, "socket error: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }
#else
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("socket");
        return;
    }
#endif

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons((unsigned short)self_node.port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp_sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
#ifdef _WIN32
        fprintf(stderr, "bind error: %d\n", WSAGetLastError());
        closesocket(udp_sock);
        WSACleanup();
#else
        perror("bind");
        close(udp_sock);
#endif
        return;
    }

    printf("[%c] Listening on port %d. Type 'send <DST> <msg>' or 'quit'.\n",
           self_node.id, self_node.port);
    print_routing_table();

#ifdef _WIN32
    InitializeCriticalSection(&q_lock);
    input_thread = CreateThread(NULL, 0, stdin_thread, NULL, 0, NULL);
    if (input_thread == NULL) {
        fprintf(stderr, "CreateThread failed\n");
        closesocket(udp_sock);
        WSACleanup();
        return;
    }

    while (!g_quit) {
        fd_set read_set;
        struct timeval timeout;

        if (q_pop(line) == 0) {
            if (strcmp(line, "quit") == 0) {
                g_quit = 1;
                break;
            }
            handle_command(line);
        }

        FD_ZERO(&read_set);
        FD_SET(udp_sock, &read_set);
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000;

        if (select(0, &read_set, NULL, NULL, &timeout) > 0 &&
            FD_ISSET(udp_sock, &read_set)) {
            int n = recvfrom(udp_sock, udp_buf, sizeof(udp_buf) - 1,
                             0, NULL, NULL);
            if (n > 0) {
                udp_buf[n] = '\0';
                forward_message(udp_buf);
            }
        }

        Sleep(5);
    }

    g_quit = 1;
    WaitForSingleObject(input_thread, 1000);
    CloseHandle(input_thread);
    DeleteCriticalSection(&q_lock);
    closesocket(udp_sock);
    WSACleanup();
#else
    while (1) {
        fd_set read_set;
        int max_fd;
        int ret;

        FD_ZERO(&read_set);
        FD_SET(STDIN_FILENO, &read_set);
        FD_SET(udp_sock, &read_set);
        max_fd = udp_sock > STDIN_FILENO ? udp_sock : STDIN_FILENO;

        ret = select(max_fd + 1, &read_set, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_set)) {
            if (fgets(line, sizeof(line), stdin) == NULL) {
                break;
            }

            line[strcspn(line, "\r\n")] = '\0';
            if (strcmp(line, "quit") == 0) {
                break;
            }

            handle_command(line);
        }

        if (FD_ISSET(udp_sock, &read_set)) {
            int n = (int)recvfrom(udp_sock, udp_buf, sizeof(udp_buf) - 1,
                                  0, NULL, NULL);
            if (n > 0) {
                udp_buf[n] = '\0';
                forward_message(udp_buf);
            }
        }
    }

    close(udp_sock);
#endif
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <conf_file>\n", argv[0]);
        return 1;
    }

    if (parse_conf(argv[1]) != 0) {
        return 1;
    }

    dijkstra(self_node.idx);
    run_node();

    return 0;
}
