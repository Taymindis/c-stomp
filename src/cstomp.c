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
#define cstmp_buf_left(b) (size_t) ( (b->start + b->total_size) - b->last)
#define LF_CHAR     (u_char) '\n'
#define LF     (u_char*) "\n"
#define CRLF   (u_char*)"\r\n"
#define C_STMP_POLL_ERR         (-1)
#define C_STMP_POLL_EXPIRE      (0)
#define C_STMP_WRITE_CTR_AT_LF(fd)  send(fd, "\0\n", 2, 0)

/***
*  Sharing the socket for read and write will make the things split up
*  Make sure each socket only proceed one frame fully requested
**/
#ifdef CSTOMP_READ_WRITE_SHR_LOCK
#define CSTMP_LOCK_READING while(__sync_lock_test_and_set(&sess->read_lock, 1))
#define CSTMP_RELEASE_READING __sync_lock_release(&sess->read_lock)
#define CSTMP_LOCK_WRITING while(__sync_lock_test_and_set(&sess->write_lock, 1))
#define CSTMP_RELEASE_WRITING __sync_lock_release(&sess->write_lock)
#else
#define CSTMP_LOCK_READING
#define CSTMP_RELEASE_READING
#define CSTMP_LOCK_WRITING
#define CSTMP_RELEASE_WRITING
#endif

#define CHECK_ERROR(n) \
if(n<0){\
if (errno == EWOULDBLOCK || errno == EINTR) {\
continue;\
}else if (errno != EAGAIN){\
fprintf(stderr, "Error while process socket read/write: %s\n",strerror(errno));\
tries=0;success=0;/*FAIL*/\
}}

#define CHECK_OR_GOTO(n, __step) \
if(n<0){\
if ((errno == EWOULDBLOCK || errno == EINTR) && tries--) {\
goto __step;\
}else if (errno != EAGAIN){\
fprintf(stderr, "Error while process socket read/write: %s\n",strerror(errno));\
tries=0;success=0;/*FAIL*/\
}}

#define FRAME_READ_RETURN(success) \
CSTMP_RELEASE_READING;\
if(!success){\
fprintf(stderr, "%s\n", "Error, Invalid frame IO reading");\
}return success;


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
    return cstmp_connect_t(hostname, port, 3000, 3000);
}

cstmp_session_t*
cstmp_connect_t(const char *hostname, int port, int send_timeout, int recv_timeout ) {
    int       connfd;
    struct sockaddr_in *servaddr;
    size_t sizeofaddr;
    struct hostent *hostip;
    cstmp_session_t* sess = NULL;

    sess = __cstmp_alloc__(__stp_arg__, sizeof(cstmp_session_t));

    if (sess == NULL) {
        fprintf( stderr, "%s\n", "Err: No enough memory allocated");
        return NULL;
    }

    servaddr = &sess->addr;
    sizeofaddr = sizeof(sess->addr);

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

    struct timeval send_tmout_val;
    send_tmout_val.tv_sec = send_timeout / 1000; // Default 1 sec time out
    send_tmout_val.tv_usec = (send_timeout % 1000) * 1000 ;
    if (setsockopt (connfd, SOL_SOCKET, SO_SNDTIMEO, &send_tmout_val,
                    sizeof(send_tmout_val)) < 0)
        fprintf(stderr, "%s\n", "setsockopt send_tmout_val failed\n");

    struct timeval recv_tmout_val;
    recv_tmout_val.tv_sec = recv_timeout / 1000; // Default 1 sec time out
    recv_tmout_val.tv_usec = (recv_timeout % 1000) * 1000 ;
    if (setsockopt (connfd, SOL_SOCKET, SO_RCVTIMEO, &recv_tmout_val,
                    sizeof(recv_tmout_val)) < 0)
        fprintf(stderr, "%s\n", "setsockopt recv_tmout_val failed\n");

    if ( connect( connfd, ( struct sockaddr *  )servaddr, sizeofaddr ) < 0 ) {
        fprintf( stderr, "Error: unable to connect to %s:%d\n", hostname, port);
        ROLLBACK_SESSION(sess);
        return NULL;
    }

    sess->sock = connfd;
#ifdef CSTOMP_READ_WRITE_SHR_LOCK
    sess->read_lock = 0;
    sess->write_lock = 0;
    
#else
    fprintf(stderr, "%s\n", "Not allowed Read write sharing");   
#endif
    sess->send_timeout = send_timeout;
    sess->recv_timeout = recv_timeout;

    // int flags = fcntl(connfd, F_GETFL, 0);
    // flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    // if (fcntl(connfd, F_SETFL, flags) != 0)
    // {
    //     fprintf(stderr, "Error setting non-blocking mode on socket: %s\n",
    //             strerror(errno));
    //     return NULL;
    // }

    // sess->pfds[0].fd = connfd;
    // sess->pfds[0].events = POLLIN | POLLOUT;

    return sess;
}

/**To create new socket, prevent concurrent issue**/
cstmp_session_t*
cstmp_new_session( cstmp_session_t* curr_sess ) {
    int       connfd;
    struct sockaddr_in *servaddr;
    cstmp_session_t* sess = NULL;
    int send_timeout = curr_sess->send_timeout,
        recv_timeout = curr_sess->recv_timeout;

    sess = __cstmp_alloc__(__stp_arg__, sizeof(cstmp_session_t));

    if (sess == NULL) {
        fprintf( stderr, "%s\n", "Err: No enough memory allocated");
        return NULL;
    }


    if ( ( connfd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 ) {
        fprintf( stderr, "%s\n", "Error: Unable to create socket");
        ROLLBACK_SESSION(sess);
        return NULL;
    }


    if ( connect( connfd, ( struct sockaddr *  )&curr_sess->addr, sizeof(curr_sess->addr) ) < 0 ) {
        fprintf( stderr, "Error: unable to connect while creating new session\n");
        ROLLBACK_SESSION(sess);
        return NULL;
    }

    struct timeval send_tmout_val;
    send_tmout_val.tv_sec = send_timeout / 1000; // Default 1 sec time out
    send_tmout_val.tv_usec = (send_timeout % 1000) * 1000 ;
    if (setsockopt (connfd, SOL_SOCKET, SO_SNDTIMEO, &send_tmout_val,
                    sizeof(send_tmout_val)) < 0)
        fprintf(stderr, "%s\n", "setsockopt send_tmout_val failed\n");

    struct timeval recv_tmout_val;
    recv_tmout_val.tv_sec = recv_timeout / 1000; // Default 1 sec time out
    recv_tmout_val.tv_usec = (recv_timeout % 1000) * 1000 ;
    if (setsockopt (connfd, SOL_SOCKET, SO_RCVTIMEO, &recv_tmout_val,
                    sizeof(recv_tmout_val)) < 0)
        fprintf(stderr, "%s\n", "setsockopt recv_tmout_val failed\n");

    sess->addr = sess->addr;
    sess->sock = connfd;
#ifdef CSTOMP_READ_WRITE_SHR_LOCK
    sess->read_lock = 0;
    sess->write_lock = 0;
#else
    fprintf(stderr, "%s\n", "Not allowed Read write sharing");    
#endif
    sess->send_timeout = send_timeout;
    sess->recv_timeout = recv_timeout;

    return sess;
}

/** Do take note that if you disc the session, the frame instance is not longer valid **/
void
cstmp_disconnect(cstmp_session_t* stp_sess) {
    if (stp_sess) {
        shutdown(stp_sess->sock, SHUT_RDWR);
        close(stp_sess->sock);
        __cstmp_free__(__stp_arg__, stp_sess);
    }
}

cstmp_frame_t*
cstmp_new_frame() {
    cstmp_frame_buf_t *headers, *body;

    cstmp_frame_t *fr =  __cstmp_alloc__(__stp_arg__, sizeof(cstmp_frame_t));
    if (fr == NULL) {
        return NULL;
    }
    fr->cmd = "";
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
cstmp_add_header_str_and_len(cstmp_frame_t *fr, u_char *keyval, size_t keyval_len) {
    cstmp_frame_buf_t *headers;

    if (!keyval || !keyval_len)
        return 0;

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

int
cstmp_add_body_content_and_len(cstmp_frame_t *fr, u_char* content, size_t content_len) {
    cstmp_frame_buf_t *body;
    if (!content)
        return 0;

    body = &fr->body;

    if ( ( cstmp_buf_size(body) + content_len ) >  body->total_size ) {
        _cstmp_reload_buf_size(body, cstmp_buf_size(body) + content_len);
    }

    body->last = cstmp_cpymem(body->last, content, content_len);
    return 1;
}

u_char*
cstmp_get_cmd(cstmp_frame_t *fr) {
    if (fr) {
        return fr->cmd;
    }
    return "";
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
        hdr_val->data = ++ret;
        hdr_val->len = ((u_char*)strchr(hdr_val->data, LF_CHAR)) - hdr_val->data;
        return 1;
    } else {
        hdr_val->data = NULL;
        hdr_val->len = 0;
        return 0;
    }
}

void
cstmp_get_body(cstmp_frame_t *fr, cstmp_frame_val_t *body_val) {
    body_val->data = fr->body.start;
    body_val->len = cstmp_buf_size((&fr->body));
}

static void
cstmp_parse_cmd(cstmp_frame_t *fr, u_char* cmd) {
    static size_t cmd_size = sizeof(__cstmp_commands) / sizeof(u_char*) - 1;
    u_char **cmds = (u_char**) __cstmp_commands;
    int i;
    fr->cmd = ""; // empty cmd by default

    /***only 15 valid commands*/
    for (i = 0; i < cmd_size; i++) {
        if (strcmp(cmds[i], cmd) == 0) {
            fr->cmd = cmds[i];
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
    printf("%s", fr->cmd);
    printf("\\n");
    cstmp_print_raw(fr->headers.start, cstmp_buf_size((&fr->headers)));
    printf("\\n");
    cstmp_print_raw(fr->body.start, cstmp_buf_size((&fr->body)));
    printf("\n");
}

void
cstmp_dump_frame_pretty(cstmp_frame_t *fr) {
    printf("%s\n", fr->cmd);
    printf("%.*s\n\n", (int) cstmp_buf_size((&fr->headers)), fr->headers.start);
    printf("%.*s\n", (int)cstmp_buf_size((&fr->body)), fr->body.start);
    printf("\n");
}

void
cstmp_reset_frame(cstmp_frame_t *fr) {
    if (fr) {
        fr->cmd = "";
        memset(fr->headers.start, 0, cstmp_buf_size((&fr->headers)));
        memset(fr->body.start, 0, cstmp_buf_size((&fr->body)));
        fr->headers.last = fr->headers.start;
        fr->body.last = fr->body.start;
    }
}

int
cstmp_send_direct(cstmp_session_t *sess, const u_char *frame_str, int tries) {
    int success = 0, connfd, rv;
    if (sess) {
        connfd = sess->sock;
        CSTMP_LOCK_WRITING;
        do {
            if ( (rv = send(connfd, frame_str, strlen(frame_str), 0)) < 0 ||
                    (rv = C_STMP_WRITE_CTR_AT_LF(connfd)) < 0) {
                CHECK_ERROR(rv);
            } else {
                success = 1;
                tries = 0;
            }
        } while (tries--); /*while try*/
        CSTMP_RELEASE_WRITING;
    }
    return success;
}

int
cstmp_send(cstmp_session_t *sess, cstmp_frame_t *fr, int tries) {
    int success = 0, connfd, rv;
    if (fr && sess) {
        connfd = sess->sock;
        const u_char* cmd = fr->cmd;
        const size_t header_len = cstmp_buf_size((&fr->headers)), body_len = cstmp_buf_size((&fr->body));
        CSTMP_LOCK_WRITING;
        do {
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
        } while (tries--);/*while try*/
        CSTMP_RELEASE_WRITING;
    } else fprintf(stderr, "%s\n", "Invalid Frame or session type");
    return success;
}

int
cstmp_recv(cstmp_session_t *sess, cstmp_frame_t *fr, int tries) {
    int success = 0, connfd, content_len_i;
    u_char* content_len_s;
    if (fr && sess) {
        u_char cmd_buff[1], cmd[12];
        cstmp_reset_frame(fr);
        connfd = sess->sock;
        int n, i = 0;
        CSTMP_LOCK_READING;
        do {
            /*Parse Cmd*/
            while ( (n = recv( connfd , cmd_buff, 1, 0)) > 0) {
                if (cmd_buff[0] == '\n') {
                    cmd[i] = '\0';
                    cstmp_parse_cmd(fr, cmd);
                    break;
                } else if (i == 12) {
                    FRAME_READ_RETURN(0);
                }
                cmd[i++] = cmd_buff[0];
            }
            CHECK_ERROR(n);
            /***parse Header ***/
            u_char last_char = 0;
            cstmp_frame_buf_t *headers = &fr->headers;

            while ((n = recv( connfd , headers->last, 1, 0)) > 0) {
                if (*headers->last == '\n' && last_char == '\n') {
                    _cstmp_add_buf(headers, "\0", 1 * sizeof(u_char) );

                    /***Add Body ***/
                    cstmp_frame_buf_t *body = &fr->body;
                    if ( content_len_s = strstr(headers->start, "content-length:") ) {
                        content_len_i = atoi(content_len_s + 15 /* sizeof content-length: */ );
                        if (content_len_i > cstmp_buf_left(body)) {
                            _cstmp_reload_buf_size(body, (size_t) content_len_i);
                        }
REREAD_WHOLE_BODY:
                        if ((n = recv( connfd , body->last, content_len_i, 0)) > 0) {
                            body->last += n;
                            char terminator_linecheck[2];
                            if (((n = recv( connfd , terminator_linecheck, 2, 0)) > 1) &&
                                    terminator_linecheck[0] == 0 && terminator_linecheck[1] == '\n') {
                                FRAME_READ_RETURN(1);
                            }
                            FRAME_READ_RETURN(0);
                        }
                        CHECK_OR_GOTO(n, REREAD_WHOLE_BODY);
                    } else {
REREAD_BODY:
                        while ((n = recv( connfd , body->last, 1, 0)) > 0) {
                            if (*body->last == 0) {
                                char recv_buff[1];
                                /** Check the Frame Terminator**/
                                if (((n = recv( connfd , recv_buff, 1, 0)) > 0)) {
                                    if (recv_buff[0] != '\n') {
                                        FRAME_READ_RETURN(0);
                                    }
                                    FRAME_READ_RETURN(1);
                                }
                            }
                            *body->last++;
                            if (cstmp_buf_size(body) == body->total_size) {
                                _cstmp_reload_buf_size(body, body->total_size * 2);
                            }
                        }
                        CHECK_OR_GOTO(n, REREAD_BODY);
                    }
                    FRAME_READ_RETURN(0);
                }
                last_char = *headers->last++; /*only plus 1*/
                if (cstmp_buf_size(headers) == headers->total_size) {
                    _cstmp_reload_buf_size(headers, headers->total_size * 2);
                }
            }
            /** end of file **/
            CHECK_ERROR(n);
        } while (tries--);/*while try*/
        CSTMP_RELEASE_READING;
    } else fprintf(stderr, "%s\n", "Invalid Frame type");
    return success; /*Failed*/
}

void
cstmp_consume(cstmp_session_t *sess, cstmp_frame_t *fr, void (*callback)(cstmp_frame_t *), int *consuming) {
    while (*consuming) {
        if (cstmp_recv(sess, fr, 0)) {
            callback(fr);
        }
    }
}
