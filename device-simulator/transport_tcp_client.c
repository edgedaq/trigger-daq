// transport_tcp.c — TCP server transport with non-blocking recv/send
#include "transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
  #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
  typedef SOCKET socket_t;
  #define CLOSESOCK closesocket
  #define SOCKERR()  WSAGetLastError()
  static int would_block(int e){ return e == WSAEWOULDBLOCK; }
  static void set_nonblock(socket_t s){ u_long m = 1; ioctlsocket(s, FIONBIO, &m); }
  static void sleep_ms(unsigned ms){ Sleep(ms); }
#else
  #include <unistd.h>
  #include <errno.h>
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <fcntl.h>
  typedef int socket_t;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR   (-1)
  #define CLOSESOCK close
  #define SOCKERR()  errno
  static int would_block(int e){ return (e == EAGAIN || e == EWOULDBLOCK); }
  static void set_nonblock(socket_t s){
      int fl = fcntl(s, F_GETFL, 0); if (fl < 0) fl = 0;
      fcntl(s, F_SETFL, fl | O_NONBLOCK);
  }
  static void sleep_ms(unsigned ms){ usleep(ms * 1000); }
#endif

typedef struct {
    socket_t listen_fd;
    socket_t client_fd;
    int      port;
    char     bind_host[64]; // optional bind address, e.g., "0.0.0.0"
} tcp_context_t;

/* ---------- helpers ---------- */
static void parse_addr(const char* cfg, char* host, int host_sz, int* port_out){
    // defaults
    snprintf(host, host_sz, "%s", "0.0.0.0");
    *port_out = 9001;

    if (!cfg || !*cfg) return;

    // strip scheme like "tcp://"
    const char* p = strstr(cfg, "://");
    if (p) cfg = p + 3;

    const char* colon = strrchr(cfg, ':');
    if (colon){
        int hlen = (int)(colon - cfg);
        if (hlen > 0 && hlen < host_sz){
            memcpy(host, cfg, hlen);
            host[hlen] = '\0';
        }
        int prt = atoi(colon + 1);
        if (prt > 0 && prt <= 65535) *port_out = prt;
        return;
    }

    // no colon => maybe just a port number
    int all_digit = 1;
    for (const char* q = cfg; *q; ++q){
        if (*q < '0' || *q > '9'){ all_digit = 0; break; }
    }
    if (all_digit){
        int prt = atoi(cfg);
        if (prt > 0 && prt <= 65535) *port_out = prt;
        return;
    }

    // otherwise treat as host only, keep default port
    snprintf(host, host_sz, "%s", cfg);
}

static void log_client_addr(struct sockaddr_in* cli){
#ifdef _WIN32
    char ip[64]; const char* s = inet_ntoa(cli->sin_addr);
    if (!s) s = "?";
    snprintf(ip, sizeof(ip), "%s", s);
    printf("[TCP] Client connected: %s:%d\n", ip, (int)ntohs(cli->sin_port));
#else
    char ip[64];
    inet_ntop(AF_INET, &cli->sin_addr, ip, sizeof(ip));
    printf("[TCP] Client connected: %s:%d\n", ip, (int)ntohs(cli->sin_port));
#endif
}

/* ---------- transport impl ---------- */
static int tcp_init(void* ctx, const char* config){
    tcp_context_t* t = (tcp_context_t*)ctx;
    memset(t, 0, sizeof(*t));
    t->listen_fd = (socket_t)INVALID_SOCKET;
    t->client_fd = (socket_t)INVALID_SOCKET;

#ifdef _WIN32
    WSADATA wsa; 
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0){
        printf("[TCP] WSAStartup failed\n");
        return -1;
    }
#endif

    parse_addr(config, t->bind_host, (int)sizeof(t->bind_host), &t->port);

    t->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (t->listen_fd == (socket_t)INVALID_SOCKET){
        printf("[TCP] socket() failed\n");
        return -1;
    }

    int opt = 1;
    setsockopt(t->listen_fd, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, (socklen_t)sizeof(opt));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)t->port);

    // bind to requested host or ANY
    if (strcmp(t->bind_host, "0.0.0.0") == 0){
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, t->bind_host, &sa.sin_addr) != 1){
            printf("[TCP] inet_pton failed for '%s', fallback ANY\n", t->bind_host);
            sa.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    }

    if (bind(t->listen_fd, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR){
        printf("[TCP] bind(%s:%d) failed (err=%d)\n", t->bind_host, t->port, SOCKERR());
        CLOSESOCK(t->listen_fd);
        t->listen_fd = (socket_t)INVALID_SOCKET;
        return -1;
    }
    if (listen(t->listen_fd, 1) == SOCKET_ERROR){
        printf("[TCP] listen() failed (err=%d)\n", SOCKERR());
        CLOSESOCK(t->listen_fd);
        t->listen_fd = (socket_t)INVALID_SOCKET;
        return -1;
    }

    printf("[TCP] Listening on %s:%d ...\n", t->bind_host, t->port);
    return 0;
}

static int tcp_wait_connection(void* ctx){
    tcp_context_t* t = (tcp_context_t*)ctx;

    struct sockaddr_in cli;
    socklen_t len = (socklen_t)sizeof(cli);
    memset(&cli, 0, sizeof(cli));

    // 阻塞等待一个连接
#ifdef _WIN32
    t->client_fd = accept(t->listen_fd, (struct sockaddr*)&cli, &len);
#else
    t->client_fd = accept(t->listen_fd, (struct sockaddr*)&cli, &len);
#endif
    if (t->client_fd == (socket_t)INVALID_SOCKET){
        printf("[TCP] accept() failed (err=%d)\n", SOCKERR());
        return -1;
    }

    // 关键：把已接受到的 client 设为非阻塞
    set_nonblock(t->client_fd);
    log_client_addr(&cli);
    return 0;
}

static int tcp_recv(void* ctx, uint8_t* buf, int len){
    tcp_context_t* t = (tcp_context_t*)ctx;
    if (!buf || len <= 0) return -1;
    if (t->client_fd == (socket_t)INVALID_SOCKET) return -1;

#ifdef _WIN32
    int n = recv(t->client_fd, (char*)buf, len, 0);
#else
    int n = recv(t->client_fd, buf, len, 0);
#endif

    if (n > 0) return n;
    if (n == 0){
        printf("[TCP] Client closed connection\n");
        return -1;
    }

    int e = SOCKERR();
    if (would_block(e)) return 0; // 非阻塞：当前无数据
    // 其他错误
    // printf("[TCP] recv error=%d\n", e);
    return -1;
}

static int tcp_send(void* ctx, const uint8_t* buf, int len){
    tcp_context_t* t = (tcp_context_t*)ctx;
    if (!buf || len <= 0) return -1;
    if (t->client_fd == (socket_t)INVALID_SOCKET) return -1;

    int sent = 0;
    while (sent < len){
#ifdef _WIN32
        int n = send(t->client_fd, (const char*)buf + sent, len - sent, 0);
#else
        int n = send(t->client_fd, buf + sent, len - sent, 0);
#endif
        if (n > 0){
            sent += n;
            continue;
        }
        if (n == 0){
            // 极少见：视为对端关闭
            return -1;
        }
        int e = SOCKERR();
        if (would_block(e)){
            // 写缓冲满，稍等再试，避免拆帧
            sleep_ms(1);
            continue;
        }
        // 其他错误
        // printf("[TCP] send error=%d\n", e);
        return -1;
    }
    return sent;
}

static void tcp_cleanup(void* ctx){
    tcp_context_t* t = (tcp_context_t*)ctx;
    if (t->client_fd != (socket_t)INVALID_SOCKET){
        CLOSESOCK(t->client_fd);
        t->client_fd = (socket_t)INVALID_SOCKET;
    }
    if (t->listen_fd != (socket_t)INVALID_SOCKET){
        CLOSESOCK(t->listen_fd);
        t->listen_fd = (socket_t)INVALID_SOCKET;
    }
#ifdef _WIN32
    WSACleanup();
#endif
    memset(t, 0, sizeof(*t));
}

/* ---------- factory ---------- */
transport_t* transport_tcp_create(void){
    static tcp_context_t g_ctx;
    static transport_t   g_tp = {
        .init            = tcp_init,
        .wait_connection = tcp_wait_connection,
        .recv            = tcp_recv,
        .send            = tcp_send,
        .cleanup         = tcp_cleanup,
        .impl_ctx        = &g_ctx
    };
    return &g_tp;
}
