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

/**VALID STOMP COMMAND**/
    // "SEND",
    // "SUBSCRIBE",
    // "UNSUBSCRIBE",
    // "BEGIN",
    // "COMMIT",
    // "ABORT",
    // "ACK",
    // "NACK",
    // "DISCONNECT",
    // "CONNECT",
    // "STOMP",
    // "CONNECTED",
    // "MESSAGE",
    // "RECEIPT",
    // "ERROR",
/*********/

/** 
* Noted: 
* cstmp_readwrite_frame are safe to use if you are connect 1 frame per connection session.
* 
* If session being sharing session to multiple frame, only each frame should only have one type either read or write only
*
**/
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
    int sock;
    struct sockaddr_in addr;
    int send_timeout;
    int recv_timeout;
    /*Atomic*/int read_lock;
    /*Atomic*/int write_lock;
} cstmp_session_t;

typedef struct cstmp_frame_s {
    u_char *cmd;
    cstmp_frame_buf_t headers;
    cstmp_frame_buf_t body;
    cstmp_session_t *sess;
} cstmp_frame_t;


/** Extra malloc and free customization **/
extern void cstmp_set_malloc_management(void* (*stp_alloc)(void* arg, size_t sz), void (*stp_free)(void* arg, void* ptr), void* arg );

extern cstmp_session_t* cstmp_connect(const char *hostname, int port );
extern cstmp_session_t* cstmp_connect_t(const char *hostname, int port, int send_timeout, int recv_timeout );
extern cstmp_session_t* cstmp_new_session( cstmp_session_t* curr_sess );


/** Do take note that if you disc the session, some other frame instance might using it**/
extern void cstmp_disconnect(cstmp_session_t* stp_sess);

/**To create new socket, prevent concurrent issue**/
extern cstmp_frame_t* cstmp_new_frame();

extern void cstmp_destroy_frame(cstmp_frame_t *fr);

extern int cstmp_add_header_str(cstmp_frame_t *fr, const u_char *keyval);

extern int cstmp_add_header_str_and_len(cstmp_frame_t *fr, u_char *keyval, size_t keyval_len);

extern int cstmp_add_header(cstmp_frame_t *fr, const u_char *key, const u_char* val);

extern int cstmp_add_body_content(cstmp_frame_t *fr, u_char* content);

extern int cstmp_add_body_content_and_len(cstmp_frame_t *fr, u_char* content, size_t content_len);

extern u_char* cstmp_get_cmd(cstmp_frame_t *fr);

extern int cstmp_get_header(cstmp_frame_t *fr, const u_char *key, cstmp_frame_val_t *hdr_val);

extern void cstmp_get_body(cstmp_frame_t *fr, cstmp_frame_val_t *body_val);

extern void cstmp_dump_frame_raw(cstmp_frame_t *fr);

extern void cstmp_dump_frame_pretty(cstmp_frame_t *fr);

extern void cstmp_reset_frame(cstmp_frame_t *fr);

extern int cstmp_send_direct(cstmp_session_t *sess, const u_char *frame_str, int tries);

extern int cstmp_send(cstmp_session_t *sess, cstmp_frame_t *fr, int tries);

extern int cstmp_recv(cstmp_session_t *sess, cstmp_frame_t *fr, int tries);

extern void cstmp_consume(cstmp_session_t *sess, cstmp_frame_t *fr, void (*callback)(cstmp_frame_t *), int *consuming);

#endif