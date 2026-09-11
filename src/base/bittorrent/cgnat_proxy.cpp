// cgnat_proxy.cpp — standalone TCP proxy + keepalive for CGNAT
// Usage: cgnat_proxy <listen_port> <backend_port> <port_check_url>
// Outputs "PORT=<ext_port>\n" on startup, then proxies.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif

static volatile int running = 1;
static void sig_handler(int) { running = 0; }

static int discover_port(int bind_port, const char *url)
{
    char host[256] = {0}, path[256] = "/";
    int port = 80;
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char *slash = strchr(p, '/'), *colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        memcpy(host, p, colon - p); port = atoi(colon + 1);
    } else if (slash) memcpy(host, p, slash - p);
    else strncpy(host, p, sizeof(host)-1);
    if (slash) strncpy(path, slash, sizeof(path)-1);

    struct hostent *he = gethostbyname(host);
    if (!he) return -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in local = {0};
    local.sin_family = AF_INET; local.sin_port = htons(bind_port); local.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) { close(fd); return -1; }

    struct sockaddr_in remote = {0};
    remote.sin_family = AF_INET; remote.sin_port = htons(port);
    memcpy(&remote.sin_addr, he->h_addr_list[0], he->h_length);
    if (connect(fd, (struct sockaddr *)&remote, sizeof(remote)) < 0) { close(fd); return -1; }

    char req[512];
    snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    send(fd, req, strlen(req), 0);

    char buf[4096]; int n = 0;
    fd_set fds; struct timeval tv = {3, 0};
    FD_ZERO(&fds); FD_SET(fd, &fds);
    if (select(fd+1, &fds, NULL, NULL, &tv) > 0) n = recv(fd, buf, sizeof(buf)-1, 0);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = 0;

    char *body = strstr(buf, "\r\n\r\n");
    if (!body) return -1;
    body += 4;
    while (*body == ' ' || *body == '\r' || *body == '\n') body++;
    return atoi(body);
}

int main(int argc, char *argv[])
{
    if (argc < 4) { fprintf(stderr, "Usage: %s <listen_port> <backend_port> <url>\n", argv[0]); return 1; }
    int lp = atoi(argv[1]), bp = atoi(argv[2]);
    const char *url = argv[3];
    signal(SIGTERM, sig_handler); signal(SIGINT, sig_handler); signal(SIGPIPE, SIG_IGN);

    // Discover
    int ext = discover_port(lp, url);
    if (ext <= 0) { fprintf(stderr, "port discovery failed\n"); return 1; }
    printf("PORT=%d\n", ext); fflush(stdout);

    // Listen socket
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(ls, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    struct sockaddr_in la = {0};
    la.sin_family = AF_INET; la.sin_port = htons(lp); la.sin_addr.s_addr = INADDR_ANY;
    if (bind(ls, (struct sockaddr *)&la, sizeof(la)) < 0 || listen(ls, 32) < 0)
    { perror("bind/listen"); return 1; }

    // Keepalive
    int kf = -1; time_t last_ka = 0;
    char ka_host[256] = {0};
    { const char *p = url; if (strncmp(p, "http://", 7) == 0) p += 7;
      const char *s = strchr(p, '/'); size_t n = s ? (size_t)(s-p) : strlen(p);
      memcpy(ka_host, p, n < 255 ? n : 255); }

    auto open_ka = [&]() -> int {
        struct hostent *he = gethostbyname(ka_host);
        if (!he) return -1;
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        struct sockaddr_in l = {0};
        l.sin_family = AF_INET; l.sin_port = htons(lp); l.sin_addr.s_addr = INADDR_ANY;
        if (bind(fd, (struct sockaddr *)&l, sizeof(l)) < 0) { close(fd); return -1; }
        struct sockaddr_in r = {0};
        r.sin_family = AF_INET; r.sin_port = htons(80);
        memcpy(&r.sin_addr, he->h_addr_list[0], he->h_length);
        if (connect(fd, (struct sockaddr *)&r, sizeof(r)) < 0) { close(fd); return -1; }
        return fd;
    };

    // Client slots
    struct { int cf, bf; } clients[64];
    memset(clients, -1, sizeof(clients));

    while (running)
    {
        fd_set rfds; FD_ZERO(&rfds);
        FD_SET(ls, &rfds); int maxfd = ls;
        if (kf >= 0) { FD_SET(kf, &rfds); if (kf > maxfd) maxfd = kf; }
        for (int i = 0; i < 64; i++) {
            if (clients[i].cf >= 0) { FD_SET(clients[i].cf, &rfds); if (clients[i].cf > maxfd) maxfd = clients[i].cf; }
            if (clients[i].bf >= 0) { FD_SET(clients[i].bf, &rfds); if (clients[i].bf > maxfd) maxfd = clients[i].bf; }
        }
        struct timeval tv = {1, 0};
        if (select(maxfd+1, &rfds, NULL, NULL, &tv) < 0) break;

        if (FD_ISSET(ls, &rfds)) {
            int cf = accept(ls, NULL, NULL);
            if (cf >= 0) {
                int bf = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in be = {0};
                be.sin_family = AF_INET; be.sin_port = htons(bp); be.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                if (bf < 0 || connect(bf, (struct sockaddr *)&be, sizeof(be)) < 0) {
                    if (bf >= 0) close(bf); close(cf);
                } else {
                    for (int i = 0; i < 64; i++) if (clients[i].cf < 0) { clients[i].cf = cf; clients[i].bf = bf; break; }
                }
            }
        }

        for (int i = 0; i < 64; i++) {
            if (clients[i].cf >= 0 && FD_ISSET(clients[i].cf, &rfds)) {
                char buf[8192]; ssize_t n = recv(clients[i].cf, buf, sizeof(buf), 0);
                if (n <= 0) { close(clients[i].cf); close(clients[i].bf); clients[i].cf = -1; }
                else if (clients[i].bf >= 0) send(clients[i].bf, buf, n, MSG_NOSIGNAL);
            }
            if (clients[i].bf >= 0 && FD_ISSET(clients[i].bf, &rfds)) {
                char buf[8192]; ssize_t n = recv(clients[i].bf, buf, sizeof(buf), 0);
                if (n <= 0) { close(clients[i].cf); close(clients[i].bf); clients[i].cf = -1; }
                else if (clients[i].cf >= 0) send(clients[i].cf, buf, n, MSG_NOSIGNAL);
            }
        }

        time_t now = time(NULL);
        if (now - last_ka > 25) {
            if (kf >= 0) {
                char head[256]; snprintf(head, sizeof(head), "HEAD / HTTP/1.0\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n", ka_host);
                if (send(kf, head, strlen(head), MSG_NOSIGNAL) < 0) { close(kf); kf = -1; }
            }
            if (kf < 0) kf = open_ka();
            last_ka = now;
        }
    }

    for (int i = 0; i < 64; i++) { if (clients[i].cf >= 0) { close(clients[i].cf); close(clients[i].bf); } }
    if (kf >= 0) close(kf); close(ls);
    return 0;
}
