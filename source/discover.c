#include "discover.h"
#include <switch.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// Common NAS HTTP ports to scan
static const int common_ports[] = {80, 8080, 8000, 5000, 5001, 9000, 9090, 7001, 7443, 443};

void discover_init(void) {
    // Socket already initialized by socketInitializeDefault() in main
}

void discover_cleanup(void) {
}

// Test if a server at ip:port responds to HTTP
static bool test_http(const char* ip, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_aton(ip, &addr.sin_addr);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    fd_set fdset;
    struct timeval tv;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 500ms timeout

    bool connected = false;
    if (select(sock + 1, NULL, &fdset, NULL, &tv) > 0) {
        int soerr = 0;
        socklen_t len = sizeof(soerr);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &len);
        connected = (soerr == 0);
    }

    if (connected) {
        // Try a simple HTTP HEAD request
        const char* req = "HEAD / HTTP/1.0\r\nHost: ";
        char buf[512];
        snprintf(buf, sizeof(buf), "%s%s\r\nConnection: close\r\n\r\n", req, ip);
        send(sock, buf, strlen(buf), MSG_DONTWAIT);

        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);
        if (select(sock + 1, &fdset, NULL, NULL, &tv) > 0) {
            char resp[512] = {0};
            int n = recv(sock, resp, sizeof(resp) - 1, 0);
            if (n > 0 && (strstr(resp, "HTTP/") || strstr(resp, "Server:"))) {
                close(sock);
                return true;
            }
        }
    }

    close(sock);
    return false;
}

bool discover_test_server(const char* ip, int port) {
    return test_http(ip, port);
}

int discover_scan(SavedServer* out_servers, int max_count) {
    int found = 0;

    // Get local IP to determine subnet
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    int tmp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (tmp_sock < 0) return 0;

    // Connect to public DNS to get local interface
    struct sockaddr_in dns;
    memset(&dns, 0, sizeof(dns));
    dns.sin_family = AF_INET;
    dns.sin_port = htons(53);
    inet_aton("8.8.8.8", &dns.sin_addr);

    if (connect(tmp_sock, (struct sockaddr*)&dns, sizeof(dns)) < 0) {
        close(tmp_sock);
        return 0;
    }

    getsockname(tmp_sock, (struct sockaddr*)&local_addr, &addr_len);
    close(tmp_sock);

    u32 local_ip = ntohl(local_addr.sin_addr.s_addr);
    u32 subnet = local_ip & 0xFFFFFF00; // /24 subnet

    // Scan 192.168.x.1-254
    for (int i = 1; i < 255 && found < max_count; i++) {
        u32 target_ip = subnet | i;
        if (target_ip == local_ip) continue;

        struct in_addr addr;
        addr.s_addr = htonl(target_ip);
        char ip_str[32];
        snprintf(ip_str, sizeof(ip_str), "%s", inet_ntoa(addr));

        // Try common NAS ports
        for (int p = 0; p < sizeof(common_ports) / sizeof(common_ports[0]) && found < max_count; p++) {
            int port = common_ports[p];
            if (test_http(ip_str, port)) {
                memset(&out_servers[found], 0, sizeof(SavedServer));
                snprintf(out_servers[found].name, MAX_TEXT_LEN, "NAS %s:%d", ip_str, port);
                snprintf(out_servers[found].url, MAX_URL_LEN, "http://%s:%d", ip_str, port);
                strncpy(out_servers[found].ip, ip_str, sizeof(out_servers[found].ip) - 1);
                out_servers[found].port = port;
                out_servers[found].type = SERVER_HTTP;
                out_servers[found].online = true;
                found++;
                break; // Only one entry per IP
            }
        }
    }

    return found;
}
