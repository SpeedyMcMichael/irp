#define _POSIX_C_SOURCE 200809L
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "config.h"

#ifndef MAXLINE
#define MAXLINE 2048
#endif

static int    sock = -1;
static WINDOW *winp;
static WINDOW *wpad;
static char   current_chan[64] = "";
static volatile sig_atomic_t running = 1;

/* Scrollback state */
static int pad_pos = 0;
static int max_lines = 1000;
static int cur_line = 0;

static void die(const char *m) {
    if (!isendwin()) endwin();
    fprintf(stderr, "irp: %s: %s\n", m, strerror(errno));
    exit(1);
}

static void on_sigint(int s) { (void)s; running = 0; }

/* ---------- base64 encoder for sasl ---------- */
static char *base64_encode(const unsigned char *src, size_t len) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *out, *pos;
    const unsigned char *end, *in;

    out = malloc(4 * ((len + 2) / 3) + 1);
    if (!out) return NULL;
    pos = out; end = src + len; in = src;

    while (end - in >= 3) {
        *pos++ = table[in[0] >> 2];
        *pos++ = table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = table[in[2] & 0x3f];
        in += 3;
    }
    if (end - in > 0) {
        *pos++ = table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
            *pos++ = table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }
    *pos = '\0';
    return out;
}

/* ---------- networking ---------- */
static int tcp_connect(const char *host, const char *port) {
    struct addrinfo hints = {0}, *res, *p;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;
    int fd = -1;
    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

static void irc_send(const char *fmt, ...) {
    char buf[MAXLINE];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    buf[n++] = '\r'; buf[n++] = '\n';
    send(sock, buf, n, 0);
}

/* ---------- ui and rendering ---------- */
static void update_pad(void) {
    int r, c;
    getmaxyx(stdscr, r, c);
    prefresh(wpad, pad_pos, 0, 0, 0, r - 3, c - 1);
}

static void log_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vw_printw(wpad, fmt, ap);
    va_end(ap);
    waddch(wpad, '\n');
    cur_line++;

    int r, c;
    getmaxyx(stdscr, r, c);
    // Auto-scroll to bottom if we are at the edge
    if (pad_pos >= cur_line - r - 2) {
        pad_pos = (cur_line > r - 2) ? cur_line - (r - 2) : 0;
    }
    update_pad();
}

static void ui_init(void) {
    initscr();
    start_color();
    init_pair(1, FG_COLOR, BG_COLOR);
    cbreak(); noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    int r, c;
    getmaxyx(stdscr, r, c);
    wpad = newpad(max_lines, c);
    winp = newwin(1, c, r - 1, 0);
    wbkgd(wpad, COLOR_PAIR(1));
    wbkgd(winp, COLOR_PAIR(1));
    scrollok(wpad, TRUE);
    
    attron(COLOR_PAIR(1));
    mvhline(r - 2, 0, '=', c);
    attroff(COLOR_PAIR(1));
    refresh();
}

/* ---------- irc and input logic ---------- */
static void handle_server_msg(char *line) {
    if (!line || !*line) return;

    if (strstr(line, "ACK :sasl")) {
        irc_send("AUTHENTICATE PLAIN");
    } else if (strstr(line, "AUTHENTICATE +")) {
        unsigned char plain[512];
        int len = snprintf((char*)plain, sizeof(plain), "%s", USERNAME);
        plain[len++] = '\0';
        len += snprintf((char*)plain + len, sizeof(plain) - len, "%s", USERNAME);
        plain[len++] = '\0';
        len += snprintf((char*)plain + len, sizeof(plain) - len, "%s", PASS);
        char *enc = base64_encode(plain, len);
        irc_send("AUTHENTICATE %s", enc);
        free(enc);
    } else if (strstr(line, "903") || strstr(line, "904")) {
        irc_send("CAP END");
    }

    if (strncmp(line, "PING ", 5) == 0) {
        line[1] = 'O';
        send(sock, line, strlen(line), 0);
        send(sock, "\r\n", 2, 0);
        return;
    }
    log_msg("%s", line);
}

static void handle_user_input(void) {
    static char inbuf[MAXLINE];
    static int pos = 0;
    int r, c;
    getmaxyx(stdscr, r, c);
    int ch = getch();

    if (ch == ERR) return;

    if (ch == KEY_PPAGE) {
        if (pad_pos > 0) pad_pos--;
    } else if (ch == KEY_NPAGE) {
        if (pad_pos < cur_line - (r - 2)) pad_pos++;
    } else if (ch == '\n' || ch == '\r') {
        inbuf[pos] = '\0';
        if (pos > 0) {
            if (inbuf[0] == '/') {
                if (strncmp(inbuf, "/j ", 3) == 0) {
                    irc_send("JOIN %s", inbuf + 3);
                    strncpy(current_chan, inbuf + 3, sizeof(current_chan)-1);
                } else if (strncmp(inbuf, "/msg ", 5) == 0) {
                    char *t = inbuf + 5, *m = strchr(t, ' ');
                    if (m) { *m = '\0'; m++; irc_send("PRIVMSG %s :%s", t, m); }
                } else if (strcmp(inbuf, "/q") == 0) running = 0;
            } else if (*current_chan) {
                irc_send("PRIVMSG %s :%s", current_chan, inbuf);
                log_msg("<%s> %s", NICK, inbuf);
            }
        }
        pos = 0; wclear(winp);
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
        if (pos > 0) pos--;
    } else if (pos < (int)sizeof(inbuf) - 1 && ch >= 32 && ch <= 126) {
        inbuf[pos++] = ch;
    }

    update_pad();
    wmove(winp, 0, 0); wclrtoeol(winp);
    mvwaddnstr(winp, 0, 0, inbuf, pos);
    wrefresh(winp);
}

int main(void) {
    signal(SIGINT, on_sigint);
    sock = tcp_connect(SERVER, PORT);
    if (sock < 0) die("connection failed");

    ui_init();
    irc_send("CAP REQ :sasl");
    irc_send("NICK %s", NICK);
    irc_send("USER %s 0 * :%s", USERNAME, REALNAME);

    char net[4096];
    int nb = 0;
    while (running) {
        fd_set fds; FD_ZERO(&fds);
        FD_SET(sock, &fds); FD_SET(STDIN_FILENO, &fds);
        if (select(sock + 1, &fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            die("select error");
        }
        if (FD_ISSET(sock, &fds)) {
            int r = recv(sock, net + nb, sizeof(net) - nb - 1, 0);
            if (r <= 0) running = 0;
            else {
                nb += r; net[nb] = '\0';
                char *p, *s = net;
                while ((p = strstr(s, "\r\n"))) {
                    *p = '\0'; handle_server_msg(s);
                    s = p + 2;
                }
                nb -= (s - net); memmove(net, s, nb);
            }
        }
        if (FD_ISSET(STDIN_FILENO, &fds)) handle_user_input();
    }
    endwin(); close(sock);
    return 0;
}
