#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "cstomp.h"

typedef void* (*cstmp_malloc_fn)(void* arg, size_t sz);
typedef void (*cstmp_free_fn)(void* arg, void* ptr);

static
void* cstmp_default_malloc(void *arg, size_t sz) {
    return malloc(sz);
}

static
void cstmp_default_free(void* arg, void* ptr) {
    free(ptr);
}

static cstmp_malloc_fn __cstmp_alloc__ = cstmp_default_malloc;
static cstmp_free_fn __cstmp_free__ = cstmp_default_free;
static void* __stp_arg__ = NULL;

#define cstmp_def_header_size 256
#define cstmp_def_message_size 1024
#define cstmp_cpymem(dst, src, n)   (((u_char *) memcpy(dst, src, n)) + (n))
#define cstmp_buf_size(b) (size_t) (b->last - b->start)
#define LF_CHAR     (u_char) '\n'
#define LF     (u_char*) "\n"
#define CRLF   (u_char*)"\r\n"
#define C_STMP_POLL_ERR         (-1)
#define C_STMP_POLL_EXPIRE      (0)
#define C_STMP_WRITE_CTR_AT_LF(fd)  send(fd, "\0\n", 2, 0)

/* If we were interrupted by a signal we need to continue to flush the data */
#define CHECK_ERROR(rv) \
if(rv == C_STMP_POLL_EXPIRE){\
    continue;\
} else if(rv<0) {\
    if (errno == EINTR) {\
    continue;\
    } else if (errno != EAGAIN && errno != EWOULDBLOCK){\
    fprintf(stderr, "Error while process socket read/write: %s\n",strerror(errno));\
    return 0;/*FAIL*/\
    }\
}

static const u_char *__cstmp_commands[16] = {
    (u_char*)"SEND",
    (u_char*)"SUBSCRIBE",
    (u_char*)"UNSUBSCRIBE",
    (u_char*)"BEGIN",
    (u_char*)"COMMIT",
    (u_char*)"ABORT",
    (u_char*)"ACK",
    (u_char*)"NACK",
    (u_char*)"DISCONNECT",
    (u_char*)"CONNECT",
    (u_char*)"STOMP",
    (u_char*)"CONNECTED",
    (u_char*)"MESSAGE",
    (u_char*)"RECEIPT",
    (u_char*)"ERROR",
    (u_char*)""
};

/** Extra malloc and free customization **/
void
cstmp_set_malloc_management(void* (*stp_alloc)(void* arg, size_t sz), void (*stp_free)(void* arg, void* ptr), void* arg ) {
    if ( !stp_alloc || !stp_free ) {
        fprintf( stderr, "%s\n", "invalid memory allocation function");
    }

    __cstmp_alloc__ = stp_alloc;
    __cstmp_free__ = stp_free;
    __stp_arg__ = arg;
}

#define ROLLBACK_SESSION(sess) __cstmp_free__(__stp_arg__, sess);

/*Default*/
cstmp_session_t*
cstmp_connect(const char *hostname, int port ) {
    int       connfd, blocking = 0; /** It can't be blocked **/
    struct sockaddr_in *servaddr;
    size_t sizeofaddr;
    struct hostent *hostip;
    cstmp_session_t* sess = NULL;

    sess = __cstmp_alloc__(__stp_arg__, sizeof(cstmp_session_t));

    if (sess == NULL) {
        fprintf( stderr, "%s\n", "Err: No enough memory allocated");
    }

    servaddr = &sess->addr;
    sizeofaddr = sizeof(sess->addr);
    sess->nfds = 1;

    if ( ( connfd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 ) {
        fprintf( stderr, "%s\n", "Error: Unable to create socket");
        ROLLBACK_SESSION(sess);
        return NULL;
    }

    // initialize the address
    bzero(servaddr, sizeofaddr);
    servaddr->sin_family = AF_INET;
    servaddr->sin_port = htons(port);
    // inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    /* Connect to NR Stomp server */
    if (!(hostip = gethostbyname(hostname)))
    {
        fprintf(stderr, "Unable to determine IP address of host %s\n", hostname);
        ROLLBACK_SESSION(sess);
        return NULL;
    }

    servaddr->sin_addr = *(struct in_addr *)hostip->h_addr;

    if ( connect( connfd, ( struct sockaddr *  )servaddr, sizeofaddr ) < 0 ) {
        fprintf( stderr, "Error: unable to connect to %s:%d\n", hostname, port);
        ROLLBACK_SESSION(sess);
        return NULL;
    }

    int flags = fcntl(connfd, F_GETFL, 0);
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    if (fcntl(connfd, F_SETFL, flags) != 0)
    {
        fprintf(stderr, "Error setting non-blocking mode on socket: %s\n",
                strerror(errno));
        return NULL;
    }

    sess->pfds[0].fd = connfd;
    // sess->pfds[0].events = POLLIN | POLLOUT;

    return sess;
}

/** Do take note that if you disc the session, the frame instance is not longer valid **/
void
cstmp_disconnect(cstmp_session_t* stp_sess) {
    if (stp_sess) {
        __cstmp_free__(__stp_arg__, stp_sess);
    }
}

/** Extra malloc and free customization **/
cstmp_frame_t*
cstmp_create_frame(cstmp_session_t* stp_sess) {
    cstmp_frame_buf_t *headers, *body;

    cstmp_frame_t *fr =  __cstmp_alloc__(__stp_arg__, sizeof(cstmp_frame_t));
    if (fr == NULL) {
        return NULL;
    }
    fr->sess = stp_sess;
    fr->cmd = NOCMD;
    headers = &fr->headers;
    body = &fr->body;
    headers->start = headers->last = __cstmp_alloc__(__stp_arg__, cstmp_def_header_size * sizeof(u_char));
    if (headers->start == NULL) {
        __cstmp_free__(__stp_arg__, fr);
        return NULL;
    }
    headers->total_size = cstmp_def_header_size;

    body->start = body->last = __cstmp_alloc__(__stp_arg__, cstmp_def_message_size * sizeof(u_char));
    if (body->start == NULL) {
        __cstmp_free__(__stp_arg__, headers->start );
        __cstmp_free__(__stp_arg__, fr);
        return NULL;
    }
    body->total_size = cstmp_def_message_size;

    return fr;
}

/** Extra malloc and free customization **/
void
cstmp_destroy_frame(cstmp_frame_t *fr) {
    if (fr) {
        if (fr->headers.start)
            __cstmp_free__(__stp_arg__, fr->headers.start);
        if (fr->body.start)
            __cstmp_free__(__stp_arg__, fr->body.start);
        __cstmp_free__(__stp_arg__, fr);
    }
}

static int
_cstmp_reload_buf_size (cstmp_frame_buf_t * buf, size_t needed_size) {
    size_t new_size = buf->total_size;
    do {
        new_size *= 2;
    } while (new_size < needed_size);

    u_char *last, *start =  __cstmp_alloc__(__stp_arg__, new_size * sizeof(u_char) );
    last = cstmp_cpymem(start, buf->start, cstmp_buf_size(buf));
    __cstmp_free__(__stp_arg__, buf->start ); // remove the old buf
    buf->start = start;
    buf->last = last;
    buf->total_size = new_size;
    return 1;
}

static int
_cstmp_add_buf(cstmp_frame_buf_t * buf, const u_char *val, size_t val_len) {
    if ( ( cstmp_buf_size(buf) + val_len) >  buf->total_size ) {
        _cstmp_reload_buf_size(buf, cstmp_buf_size(buf) + val_len/*for : and LF and \0*/);
    }
    buf->last = cstmp_cpymem(buf->last, val, val_len);
    return 1;
}

int
cstmp_add_header_str(cstmp_frame_t *fr, const u_char *keyval) {
    cstmp_frame_buf_t *headers;
    size_t keyval_len;

    if (!keyval)
        return 0;

    keyval_len = strlen(keyval);

    headers = &fr->headers;

    if ( ( cstmp_buf_size(headers) + keyval_len + 2) >  headers->total_size ) {
        _cstmp_reload_buf_size(headers, cstmp_buf_size(headers) + keyval_len + 2/*for : and LF and \0*/);
    }

    headers->last = cstmp_cpymem(headers->last, keyval, keyval_len);
    headers->last = cstmp_cpymem(headers->last, LF, 1 * sizeof(u_char));
    *headers->last = '\0';
    return 1;
}

int
cstmp_add_header(cstmp_frame_t *fr, const u_char *key, const u_char* val) {
    cstmp_frame_buf_t *headers;
    size_t key_len, val_len;

    if (!key && !val)
        return 0;

    key_len = strlen(key);
    val_len = strlen(val);

    headers = &fr->headers;

    if ( ( cstmp_buf_size(headers) + key_len + val_len + 3/*for : and LF and \0*/) >  headers->total_size ) {
        _cstmp_reload_buf_size(headers, cstmp_buf_size(headers) + key_len + val_len + 3/*for : and LF and \0*/);
    }

    headers->last = cstmp_cpymem(headers->last, key, key_len);
    *headers->last++ = ':';
    headers->last = cstmp_cpymem(headers->last, val, val_len);
    headers->last = cstmp_cpymem(headers->last, LF, 1 * sizeof(u_char));
    *headers->last = '\0';
    return 1;
}

int
cstmp_add_body_content(cstmp_frame_t *fr, u_char* content) {
    cstmp_frame_buf_t *body;
    size_t body_len;
    if (!content)
        return 0;

    body_len = strlen(content);
    body = &fr->body;

    if ( ( cstmp_buf_size(body) + body_len ) >  body->total_size ) {
        _cstmp_reload_buf_size(body, cstmp_buf_size(body) + body_len);
    }

    body->last = cstmp_cpymem(body->last, content, body_len);
    return 1;
}

u_char* 
cstmp_get_cmd(cstmp_frame_t *fr) {
    static size_t cmd_size = sizeof(__cstmp_commands)/sizeof(u_char*) - 1;
    if(fr->cmd <= cmd_size) {
        return (u_char*)__cstmp_commands[fr->cmd];
    } else return "";
}

int
cstmp_get_header(cstmp_frame_t *fr, const u_char *key, cstmp_frame_val_t *hdr_val) {
    size_t klen = key ? strlen(key) : 0;
    u_char *ret = fr->headers.start;
    while ( ret = strstr(ret, key) ) {
        ret = ret + klen;
        if ( *ret == ':' ) break;
    }

    if (ret) {
        hdr_val->val = ++ret;
        hdr_val->len = ((u_char*)strchr(hdr_val->val, LF_CHAR)) - hdr_val->val;
        return 1;
    } else {
        hdr_val->val = NULL;
        hdr_val->len = 0;
        return 0;
    }
}

void
cstmp_get_body(cstmp_frame_t *fr, cstmp_frame_val_t *body_val) {
    body_val->val = fr->body.start;
    body_val->len = cstmp_buf_size((&fr->body));
}

static void
cstmp_parse_cmd(cstmp_frame_t *fr, u_char* cmd) {
    static size_t cmd_size = sizeof(__cstmp_commands)/sizeof(u_char*) - 1;
    u_char **cmds = (u_char**) __cstmp_commands;
    int i;
    fr->cmd = cmd_size; // last one is empty cmd by default

    /***only 15 valid commands*/
    for (i = 0; i < cmd_size; i++) {
        if (strcmp(cmds[i], cmd) == 0) {
            fr->cmd = i;
            break;
        }
    }
}

static void
cstmp_print_raw(u_char *str, size_t len) {
    int i;
    char c;
    for (i = 0; i < len; i++) {
        c = str[i];
        if (isprint(c))
            putchar(c); // just print printable characters
        else if (c == '\n')
            printf("\\n"); // display newline as \n
        else if (c == '\r')
            printf("\\r"); // display newline as \n
        else if (c == '\0')
            printf("\\0"); // display newline as \n
        else
            printf("%02x", c); // print everything else as a number
    }
}

void
cstmp_dump_frame_raw(cstmp_frame_t *fr) {
    printf("%s", __cstmp_commands[fr->cmd]);
    printf("\\n");
    cstmp_print_raw(fr->headers.start, cstmp_buf_size((&fr->headers)));
    printf("\\n");
    cstmp_print_raw(fr->body.start, cstmp_buf_size((&fr->body)));
    printf("\n");
}

void
cstmp_dump_frame_pretty(cstmp_frame_t *fr) {
    printf("%s\n", __cstmp_commands[fr->cmd]);
    printf("%.*s\n\n", (int) cstmp_buf_size((&fr->headers)), fr->headers.start);
    printf("%.*s\n", (int)cstmp_buf_size((&fr->body)), fr->body.start);
    printf("\n");
}

void
cstmp_reset_frame(cstmp_frame_t *fr) {
    if (fr) {
        fr->cmd = NOCMD;
        memset(fr->headers.start, 0, cstmp_buf_size((&fr->headers)));
        memset(fr->body.start, 0, cstmp_buf_size((&fr->body)));
        fr->headers.last = fr->headers.start;
        fr->body.last = fr->body.start;
    }
}

int
cstmp_send_direct(cstmp_session_t *sess, const u_char *frame_str, int timeout_ms, int tries) {
    int success = 0, connfd;
    if (sess) {
        struct pollfd *pfds = sess->pfds;
        pfds[0].events = POLLOUT;
        do {
            int rv = poll(pfds, sess->nfds,  timeout_ms);
            CHECK_ERROR(rv);

            if (pfds[0].revents & POLLOUT) {
                connfd = pfds[0].fd;
                if ( (rv = send(connfd, frame_str, strlen(frame_str), 0)) < 0 ||
                        (rv = C_STMP_WRITE_CTR_AT_LF(connfd)) < 0) {
                    CHECK_ERROR(rv);
                } else {
                    success = 1;
                    tries = 0;
                }
            }
        } while (tries--); /*while try*/
    }
    return success;
}

int
cstmp_send(cstmp_frame_t *fr, int timeout_ms, int tries) {
    int success = 0, connfd;
    if (fr) {
        cstmp_session_t *sess = fr->sess;
        if (sess) {
            struct pollfd *pfds = sess->pfds;
            pfds[0].events = POLLOUT;
            do {
                int rv = poll(pfds, sess->nfds,  timeout_ms);
                CHECK_ERROR(rv);
                if (pfds[0].revents & POLLOUT) {
                    connfd = pfds[0].fd;
                    const u_char* cmd = __cstmp_commands[fr->cmd];
                    const size_t header_len = cstmp_buf_size((&fr->headers)), body_len = cstmp_buf_size((&fr->body));
                    if (
                        (rv = send(connfd, cmd , strlen(cmd), 0)) < 0 ||
                        (rv = send(connfd, LF, 1 * sizeof(u_char), 0)) < 0 ||
                        (header_len && (rv = send(connfd, fr->headers.start , header_len, 0)) < 0) ||
                        (rv = send(connfd, LF, 1 * sizeof(u_char), 0)) < 0 ||
                        (body_len && (rv = send(connfd, fr->body.start , body_len, 0)) < 0) ||
                        (rv = C_STMP_WRITE_CTR_AT_LF(connfd)) < 0
                    ) {
                        CHECK_ERROR(rv);
                    } else {
                        success = 1;
                        tries = 0;
                    }
                }
            } while (tries--);/*while try*/
        }
    }
    return success;
}

int
cstmp_recv(cstmp_frame_t *fr, int timeout_ms, int tries) {
    int success = 0;
    if (fr) {
        cstmp_session_t *sess = fr->sess;
        if (sess) {
            struct pollfd *pfds = sess->pfds;
            u_char recv_buff[1024], cmd[12];
            pfds[0].events = POLLIN;
            cstmp_reset_frame(fr);
            do {
                int rv = poll(sess->pfds, sess->nfds,  timeout_ms);
                CHECK_ERROR(rv);
                if (pfds[0].revents & POLLIN) {
                    int n, i = 0;
                    int connfd = pfds[0].fd;
                    /*Parse Cmd*/
                    while ( (n = recv( connfd , recv_buff, 1, 0)) > 0) {
                        if (recv_buff[0] == '\n') {
                            cmd[i] = '\0';
                            cstmp_parse_cmd(fr, cmd);
                            break;
                        }
                        cmd[i++] = recv_buff[0];
                    }
                    /***parse Header ***/
                    u_char last_char = 0;
                    cstmp_frame_buf_t *headers = &fr->headers;

                    while ((n = recv( connfd , recv_buff, 1, 0)) > 0) {
                        if (*recv_buff == '\n' && last_char == '\n') {
                            // printf("%s\n", "you are here");
                            // _cstmp_add_buf(headers, recv_buff, end_hdr - recv_buff );
                            _cstmp_add_buf(headers, "\0", 1 * sizeof(u_char) );

                            /***Add Body ***/
                            cstmp_frame_buf_t *body = &fr->body;
                            while ((n = recv( connfd , recv_buff, 1024, 0)) > 0) {
                                _cstmp_add_buf(body, recv_buff, n);
                            }
                            break;
                        } else {
                            _cstmp_add_buf(headers, recv_buff, n);
                        }
                        last_char = recv_buff[0];
                    }
                    /** end of file **/
                    CHECK_ERROR(n);
                    return ++success/*Means success*/;
                }
            } while (tries--);/*while try*/
        }
    }
    return success; /*Failed*/
}

void
cstmp_consume(cstmp_frame_t *fr, void (*callback)(cstmp_frame_t *), int *consuming, int timeout_ms) {
    while (*consuming) {
        if (cstmp_recv(fr, timeout_ms, 0)) {
            callback(fr);
        }
    }
}