/*
 * node.c — 6-Node Distance-Vector Routing (UDP)
 *
 * CSE 320 Lab 5
 *
 * Her node bir .conf dosyasından port ve komşu bilgilerini okur,
 * Dijkstra ile routing tablosunu hesaplar, UDP üzerinden mesaj iletir.
 *
 * Derleme : gcc -o node node.c
 * Çalıştırma: ./node A.conf   (her node için ayrı terminalde)
 *
 * .conf dosyası formatı:
 *   node_id   <tek harf, ör. A>
 *   port      <dinleme portu, ör. 5001>
 *   neighbor  <komşu_id> <komşu_port> <link_maliyeti>
 *   ...
 *
 * Komutlar (stdin'den):
 *   send <hedef_id> <mesaj>   — hedefe mesaj yolla
 *   quit                      — programı kapat
 */

#ifdef _WIN32
  /* Windows socket / thread headers */
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  /* Link with: gcc ... -lws2_32  (MinGW flag, no pragma needed) */
  typedef int socklen_t;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/*  Sabitler                                                             */
/* ------------------------------------------------------------------ */
#define MAX_NODES   6
#define INF         9999999
#define BUF_SIZE    1024

/* Node isimleri: A=0, B=1, C=2, D=3, E=4, F=5 */
static const char NODE_NAMES[] = "ABCDEF";

/* Ödevdeki topoloji link maliyetleri (sabit, her node aynı tabloyu bilir) */
/*
 * Topology:
 *   A-B:4   A-C:7   A-D:13  A-F:5
 *   D-C:9   D-B:8
 *   B-E:3
 *   C-E:12
 *
 * Note: A->D direct cost=13, via B cost=4+8=12 => next hop is B
 */
static int TOPO[MAX_NODES][MAX_NODES] = {
    /* A   B   C   D    E    F  */
    {  0,  4,  7,  13, INF,   5 },  /* A */
    {  4,  0, INF,   8,   3, INF }, /* B */
    {  7, INF,  0,   9,  12, INF }, /* C */
    { 13,  8,  9,   0, INF, INF }, /* D */
    { INF,  3, 12, INF,   0, INF }, /* E */
    {  5, INF, INF, INF, INF,   0 } /* F */
};

/* ------------------------------------------------------------------ */
/*  Routing tablosu                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    int dist;       /* Toplam maliyet                  */
    int next_hop;   /* Bir sonraki node indeksi (-1: kendisi) */
} Route;

static Route routing_table[MAX_NODES];

/* ------------------------------------------------------------------ */
/*  Node konfigürasyonu                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    char   id;               /* Bu node'un adı (ör. 'A')              */
    int    idx;              /* 0–5 indeksi                           */
    int    port;             /* Dinleme portu                         */
    int    neighbor_port[MAX_NODES]; /* Komşuların portları (0 = yok) */
} NodeConf;

static NodeConf self;

/* ------------------------------------------------------------------ */
/*  Dijkstra                                                             */
/* ------------------------------------------------------------------ */
static void dijkstra(int src)
{
    int visited[MAX_NODES] = {0};

    for (int i = 0; i < MAX_NODES; i++) {
        routing_table[i].dist     = INF;
        routing_table[i].next_hop = -1;
    }
    routing_table[src].dist = 0;

    /* İlk adım: doğrudan komşuları yükle */
    for (int i = 0; i < MAX_NODES; i++) {
        if (i != src && TOPO[src][i] < INF) {
            routing_table[i].dist     = TOPO[src][i];
            routing_table[i].next_hop = i;
        }
    }

    for (int iter = 0; iter < MAX_NODES - 1; iter++) {
        /* En küçük dist'li ziyaret edilmemiş node'u bul */
        int u = -1;
        for (int i = 0; i < MAX_NODES; i++) {
            if (!visited[i] && routing_table[i].dist < INF) {
                if (u == -1 || routing_table[i].dist < routing_table[u].dist)
                    u = i;
            }
        }
        if (u == -1) break;
        visited[u] = 1;

        for (int v = 0; v < MAX_NODES; v++) {
            if (!visited[v] && TOPO[u][v] < INF) {
                int alt = routing_table[u].dist + TOPO[u][v];
                if (alt < routing_table[v].dist) {
                    routing_table[v].dist     = alt;
                    /* next_hop: src'den u'ya giden ilk adım */
                    routing_table[v].next_hop = routing_table[u].next_hop;
                }
            }
        }
    }
    routing_table[src].dist     = 0;
    routing_table[src].next_hop = src;
}

/* ------------------------------------------------------------------ */
/*  Routing tablosunu ekrana bas                                          */
/* ------------------------------------------------------------------ */
static void print_routing_table(void)
{
    printf("\n[%c] Routing Tablosu (Dijkstra)\n", self.id);
    printf("%-12s %-10s %-6s\n", "Destination", "Next Hop", "Cost");
    printf("-----------------------------\n");
    for (int i = 0; i < MAX_NODES; i++) {
        char dest     = NODE_NAMES[i];
        char next     = (routing_table[i].next_hop >= 0)
                          ? NODE_NAMES[routing_table[i].next_hop] : '-';
        int  cost     = routing_table[i].dist;
        if (cost >= INF)
            printf("%-12c %-10c %-6s\n", dest, next, "INF");
        else
            printf("%-12c %-10c %-6d\n", dest, next, cost);
    }
    printf("\n");
}

/* ------------------------------------------------------------------ */
/*  .conf dosyasını oku                                                   */
/* ------------------------------------------------------------------ */
static int parse_conf(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f) { perror("fopen"); return -1; }

    memset(&self, 0, sizeof(self));

    char key[32], val[32];
    while (fscanf(f, "%31s", key) == 1) {
        if (strcmp(key, "node_id") == 0) {
            fscanf(f, "%31s", val);
            self.id  = val[0];
            self.idx = (int)(strchr(NODE_NAMES, self.id) - NODE_NAMES);
        } else if (strcmp(key, "port") == 0) {
            fscanf(f, "%d", &self.port);
        } else if (strcmp(key, "neighbor") == 0) {
            char nid[4]; int nport, ncost;
            fscanf(f, "%3s %d %d", nid, &nport, &ncost);
            int ni = (int)(strchr(NODE_NAMES, nid[0]) - NODE_NAMES);
            self.neighbor_port[ni] = nport;
        }
    }
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  UDP mesaj gönder                                                      */
/* ------------------------------------------------------------------ */
static int send_udp(int dest_port, const char *msg)
{
#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) { fprintf(stderr,"socket error\n"); return -1; }
#else
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return -1; }
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((unsigned short)dest_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    sendto(sock, msg, (int)strlen(msg), 0,
           (struct sockaddr *)&addr, sizeof(addr));

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Mesajı route et                                                       */
/* ------------------------------------------------------------------ */
/*
 * Mesaj formatı (düz metin): "SRC DST ORIG_MSG"
 * Örnek: "A D hello_from_A_to_D"
 */
static void forward_message(const char *raw)
{
    char src_c[4], dst_c[4], body[BUF_SIZE];
    if (sscanf(raw, "%3s %3s %1023[^\n]", src_c, dst_c, body) < 3) return;

    int dst_idx = (int)(strchr(NODE_NAMES, dst_c[0]) - NODE_NAMES);

    if (dst_idx == self.idx) {
        /* Bu node hedeftir */
        printf("[%c] Received message from %s: %s\n", self.id, src_c, body);
        return;
    }

    int nh = routing_table[dst_idx].next_hop;
    if (nh < 0 || nh == self.idx) {
        printf("[%c] No route to %s\n", self.id, dst_c);
        return;
    }

    char nh_c = NODE_NAMES[nh];
    int  nh_port = self.neighbor_port[nh];

    if (nh_port == 0) {
        printf("[%c] Neighbor %c port unknown!\n", self.id, nh_c);
        return;
    }

    printf("[%c] Forwarding message from %s to %s, next hop %c (port %d)\n",
           self.id, src_c, dst_c, nh_c, nh_port);

    /* Aynı mesaj formatını ilet */
    char fwd[BUF_SIZE + 16];  /* +16: room for src/dst prefix bytes */
    snprintf(fwd, sizeof(fwd), "%s %s %s", src_c, dst_c, body);
    send_udp(nh_port, fwd);
}

/* ------------------------------------------------------------------ */
/*  Shared stdin command queue (thread-safe, simple ring buffer)         */
/* ------------------------------------------------------------------ */
#ifdef _WIN32
#define QUEUE_SIZE 32
static char  cmd_queue[QUEUE_SIZE][BUF_SIZE];
static int   q_head = 0, q_tail = 0;
static CRITICAL_SECTION q_lock;
static volatile int g_quit = 0;

static int q_push(const char *s) {
    EnterCriticalSection(&q_lock);
    int next = (q_tail + 1) % QUEUE_SIZE;
    if (next == q_head) { LeaveCriticalSection(&q_lock); return -1; } /* full */
    strncpy(cmd_queue[q_tail], s, BUF_SIZE - 1);
    cmd_queue[q_tail][BUF_SIZE - 1] = '\0';
    q_tail = next;
    LeaveCriticalSection(&q_lock);
    return 0;
}

static int q_pop(char *out) {
    EnterCriticalSection(&q_lock);
    if (q_head == q_tail) { LeaveCriticalSection(&q_lock); return -1; } /* empty */
    strncpy(out, cmd_queue[q_head], BUF_SIZE - 1);
    out[BUF_SIZE - 1] = '\0';
    q_head = (q_head + 1) % QUEUE_SIZE;
    LeaveCriticalSection(&q_lock);
    return 0;
}

/* Stdin reader thread: blocking fgets, works in MSYS2/MinGW PTY */
static DWORD WINAPI stdin_thread(LPVOID arg) {
    (void)arg;
    char line[BUF_SIZE];
    while (!g_quit) {
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';
        q_push(line);
        if (strcmp(line, "quit") == 0) break;
    }
    return 0;
}
#endif /* _WIN32 */

/* ------------------------------------------------------------------ */
/*  run_node: UDP listener + stdin command handler                        */
/* ------------------------------------------------------------------ */
static void run_node(void)
{
    /* ---- Create UDP socket ---- */
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == INVALID_SOCKET) { fprintf(stderr,"socket error\n"); return; }
    /* Non-blocking */
    u_long nb_mode = 1;
    ioctlsocket(udp_sock, FIONBIO, &nb_mode);
#else
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) { perror("socket"); return; }
    /* Non-blocking */
    int flags = fcntl(udp_sock, F_GETFL, 0);
    fcntl(udp_sock, F_SETFL, flags | O_NONBLOCK);
#endif

    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family      = AF_INET;
    local.sin_port        = htons((unsigned short)self.port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp_sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind"); return;
    }

    printf("[%c] Listening on port %d. Type 'send <DST> <msg>' or 'quit'.\n",
           self.id, self.port);
    print_routing_table();

#ifdef _WIN32
    /* Start dedicated stdin reader thread */
    InitializeCriticalSection(&q_lock);
    HANDLE hThread = CreateThread(NULL, 0, stdin_thread, NULL, 0, NULL);
    if (!hThread) { fprintf(stderr, "CreateThread failed\n"); return; }
#endif

    char line[BUF_SIZE];
    char udp_buf[BUF_SIZE];

    while (1) {
#ifdef _WIN32
        /* --- Check stdin queue (filled by stdin_thread) --- */
        if (q_pop(line) == 0) {
            if (strcmp(line, "quit") == 0) { g_quit = 1; break; }
            char cmd[16], dst[4], msg[BUF_SIZE];
            if (sscanf(line, "%15s %3s %1023[^\n]", cmd, dst, msg) == 3
                && strcmp(cmd, "send") == 0) {
                char pkt[BUF_SIZE + 16];  /* +16: room for id/dst prefix */
                snprintf(pkt, sizeof(pkt), "%c %s %s", self.id, dst, msg);
                forward_message(pkt);
            }
        }
        /* --- UDP receive (non-blocking) --- */
        {
            TIMEVAL tv = {0, 10000}; /* 10 ms */
            fd_set rset;
            FD_ZERO(&rset);
            FD_SET(udp_sock, &rset);
            if (select(0, &rset, NULL, NULL, &tv) > 0 &&
                FD_ISSET(udp_sock, &rset)) {
                int n = recvfrom(udp_sock, udp_buf, sizeof(udp_buf)-1, 0, NULL, NULL);
                if (n > 0) { udp_buf[n] = '\0'; forward_message(udp_buf); }
            }
        }
        Sleep(5); /* yield 5 ms */
#else
        /* POSIX: select on stdin + udp_sock */
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(STDIN_FILENO, &rset);
        FD_SET(udp_sock, &rset);
        int maxfd = udp_sock > STDIN_FILENO ? udp_sock : STDIN_FILENO;
        struct timeval tv = {0, 50000}; /* 50 ms */
        int ret = select(maxfd+1, &rset, NULL, NULL, &tv);
        if (ret > 0) {
            if (FD_ISSET(STDIN_FILENO, &rset)) {
                if (!fgets(line, sizeof(line), stdin)) break;
                line[strcspn(line, "\r\n")] = 0;
                if (strcmp(line, "quit") == 0) break;
                char cmd[16], dst[4], msg[BUF_SIZE];
                if (sscanf(line, "%15s %3s %1023[^\n]", cmd, dst, msg) == 3
                    && strcmp(cmd, "send") == 0) {
                    char pkt[BUF_SIZE];
                    snprintf(pkt, sizeof(pkt), "%c %s %s", self.id, dst, msg);
                    forward_message(pkt);
                }
            }
            if (FD_ISSET(udp_sock, &rset)) {
                int n = (int)recvfrom(udp_sock, udp_buf, sizeof(udp_buf)-1, 0, NULL, NULL);
                if (n > 0) { udp_buf[n]='\0'; forward_message(udp_buf); }
            }
        }
#endif
    }

#ifdef _WIN32
    g_quit = 1;
    WaitForSingleObject(hThread, 1000);
    CloseHandle(hThread);
    DeleteCriticalSection(&q_lock);
    closesocket(udp_sock);
    WSACleanup();
#else
    close(udp_sock);
#endif
}

/* ------------------------------------------------------------------ */
/*  main                                                                  */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Kullanim: %s <conf_file>\n", argv[0]);
        return 1;
    }

    if (parse_conf(argv[1]) != 0) return 1;

    /* Dijkstra ile routing tablosunu hesapla */
    dijkstra(self.idx);

    run_node();
    return 0;
}
