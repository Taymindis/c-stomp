
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <cstomp.h>
#include <assert.h>

int consuming = 1;
#define HOST "localhost"
#define PORT 12345
#define QUEUE_NAME "/amq/queue/stompqueue"


void *consuming_thread(void* nothing) {

    cstmp_session_t *consuming_sess = cstmp_connect(HOST, PORT);
    cstmp_frame_t *consume_fr = cstmp_create_frame(consuming_sess);
    consume_fr->cmd = CONNECT;
    cstmp_add_header(consume_fr, "login", "guest");
    cstmp_add_header(consume_fr, "passcode", "guest");
    cstmp_add_header(consume_fr, "version", "1.2");

    if (cstmp_send(consume_fr, 1000, 0) && cstmp_recv(consume_fr, 1000, 0)) {
        cstmp_dump_frame_pretty(consume_fr);
    }

    /*** remember to reset frame before prepare the new command ***/
    cstmp_reset_frame(consume_fr);

    consume_fr->cmd = SUBSCRIBE;
    cstmp_add_header(consume_fr, "destination", QUEUE_NAME);
    cstmp_add_header(consume_fr, "ack", "auto");
    cstmp_add_header(consume_fr, "id", "0");
    cstmp_add_header(consume_fr, "durable", "false");
    cstmp_send(consume_fr, 1000, 3);

    void (*consume_handler)(cstmp_frame_t* consume_fr) =
    ({
        void __fn__ (cstmp_frame_t* consume_fr) {
            assert( strlen(cstmp_get_cmd(consume_fr)) > 0 && "Error, no command found");
            cstmp_dump_frame_pretty(consume_fr);
        }
        __fn__;
    });

    /** Keep hooking until consuming = false / 0 **/
    cstmp_consume((cstmp_frame_t*) consume_fr, consume_handler, &consuming, 500);

    /** you can use direct string if you don't want frame to send **/
    if (cstmp_send_direct(consuming_sess, "UNSUBSCRIBE\nid:0\n\n", 300, 0) &&
            cstmp_recv(consume_fr, 300, 0) ) {
        cstmp_dump_frame_pretty(consume_fr);
    }


    consume_fr->cmd = DISCONNECT;
    cstmp_add_header(consume_fr, "receipt", "dummy-recv-test");
    if ( cstmp_send(consume_fr, 300, 0) &&
            cstmp_recv(consume_fr, 300, 0) ) {
        cstmp_dump_frame_pretty(consume_fr);
    }

    cstmp_destroy_frame(consume_fr);
    cstmp_disconnect(consuming_sess);

    pthread_exit(NULL);
}

int main() {

    printf("%s %s\n", "Starting stomp connection, sending and consuming", QUEUE_NAME);
    usleep(1000 * 3000);

    /** Create a thread for consuming **/
    pthread_t t;
    if (pthread_create(&t, NULL, consuming_thread, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    pthread_detach(t);


    /*** Sending ***/
    cstmp_session_t *sess = cstmp_connect(HOST, PORT);
    cstmp_frame_t *fr = cstmp_create_frame(sess);
    fr->cmd = CONNECT;
    cstmp_add_header(fr, "login", "guest");
    cstmp_add_header(fr, "passcode", "guest");
    cstmp_add_header(fr, "version", "1.2");

    cstmp_frame_val_t val;

    if (cstmp_send(fr, 1000, 0) && cstmp_recv(fr, 1000, 0)) {
        cstmp_dump_frame_pretty(fr);
    }

    /*** Send 100 times ***/
    int send_count = 100;
    while (send_count--) {
        cstmp_reset_frame(fr);
        fr->cmd = SEND;
        cstmp_add_header(fr, "destination", QUEUE_NAME);
        cstmp_add_header(fr, "persistent", "false");
        cstmp_add_header(fr, "content-type", "text/plain");
        cstmp_add_body_content(fr, "{\"my_key\":\"akjdlkajdklj2ljladasjldjasljdl@ASD2\"}");

        if (! cstmp_send(fr, 1000, 1)) {
            assert(0 && "Unable to send frame");
            break;
        }
        usleep(1000 * 50);
    }

    consuming = 0;
    printf("%s\n", "disconnecting Stomp");
    usleep(1000 * 3000);

    fr->cmd = DISCONNECT;
    cstmp_add_header(fr, "receipt", "dummy-send-test");
    if ( cstmp_send(fr, 1000, 3) && cstmp_recv(fr, 1000, 3) ) {
        cstmp_dump_frame_pretty(fr);
    }


    cstmp_destroy_frame(fr);
    cstmp_disconnect(sess);


    printf("%s\n", "Test Passed");

    return 0;
}