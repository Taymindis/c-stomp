#ifndef CSTOMP_H
#define CSTOMP_H

#include <stdio.h>
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#ifndef u_char
#define u_char unsigned char
#endif

enum cstmp_cmd_t {
    SEND = 0,
    SUBSCRIBE = 1,
    UNSUBSCRIBE = 2,
    BEGIN = 3,
    COMMIT = 4,
    ABORT = 5,
    ACK = 6,
    NACK = 7,
    DISCONNECT = 8,
    CONNECT = 9,
    STOMP = 10,
    CONNECTED = 11,
    MESSAGE = 12,
    RECEIPT = 13,
    ERROR = 14,
    NOCMD = 15
};

typedef struct cstmp_frame_val_s {
    u_char *val;
    size_t len;
} cstmp_frame_val_t;

typedef struct cstmp_frame_buf_s {
    u_char *start;
    u_char *last;
    size_t total_size;
} cstmp_frame_buf_t;

typedef struct cstmp_session_s {
    struct pollfd pfds[2];
    nfds_t nfds;
    struct sockaddr_in addr;
} cstmp_session_t;

/***Frame is not thread safe, DO NOT share the frame for multiple threads***/
typedef struct cstmp_frame_s {
    enum cstmp_cmd_t cmd;
    cstmp_frame_buf_t headers;
    cstmp_frame_buf_t body;
    cstmp_session_t *sess;
} cstmp_frame_t;


/** Extra malloc and free customization **/
extern void cstmp_set_malloc_management(void* (*stp_alloc)(void* arg, size_t sz), void (*stp_free)(void* arg, void* ptr), void* arg );

extern cstmp_session_t* cstmp_connect(const char *hostname, int port );

/** Do take note that if you disc the session, some other frame instance might using it**/
extern void cstmp_disconnect(cstmp_session_t* stp_sess);

extern cstmp_frame_t* cstmp_create_frame(cstmp_session_t* stp_sess);

extern void cstmp_destroy_frame(cstmp_frame_t *fr);

extern int cstmp_add_header_str(cstmp_frame_t *fr, const u_char *keyval);

extern int cstmp_add_header(cstmp_frame_t *fr, const u_char *key, const u_char* val);

extern int cstmp_add_body_content(cstmp_frame_t *fr, u_char* content);

extern u_char* cstmp_get_cmd(cstmp_frame_t *fr);

extern int cstmp_get_header(cstmp_frame_t *fr, const u_char *key, cstmp_frame_val_t *hdr_val);

extern void cstmp_get_body(cstmp_frame_t *fr, cstmp_frame_val_t *body_val);

extern void cstmp_dump_frame_raw(cstmp_frame_t *fr);

extern void cstmp_dump_frame_pretty(cstmp_frame_t *fr);

extern void cstmp_reset_frame(cstmp_frame_t *fr);

extern int cstmp_send_direct(cstmp_session_t *sess, const u_char *frame_str, int timeout_ms, int tries);

extern int cstmp_send(cstmp_frame_t *fr, int timeout_ms, int tries);

extern int cstmp_recv(cstmp_frame_t *fr, int timeout_ms, int tries);

extern void cstmp_consume(cstmp_frame_t *fr, void (*callback)(cstmp_frame_t *), int *consuming, int timeout_ms);

#endif