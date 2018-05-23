#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <cstomp.h>
#include <assert.h>

int consuming = 1;
#define HOST "10.2.110.202"
#define PORT 61618
#define QUEUE_NAME "/amq/queue/stompqueue"

/***
*   This example showing each frame has each connection session, it won't affect the session read write issue.
***/

void *consuming_thread(void* nothing) {

	cstmp_session_t *consuming_sess = cstmp_connect_t(HOST, PORT, 500, 500);
	if (consuming_sess) {
		cstmp_frame_t *consume_fr = cstmp_new_frame();
		consume_fr->cmd = "CONNECT";
		cstmp_add_header(consume_fr, "login", "guest");
		cstmp_add_header(consume_fr, "passcode", "guest");
		cstmp_add_header(consume_fr, "version", "1.2");

		if (cstmp_send(consuming_sess, consume_fr, 0) && cstmp_recv(consuming_sess, consume_fr, 0)) {
			cstmp_dump_frame_pretty(consume_fr);
		}

		/*** remember to reset frame before prepare the new command ***/
		cstmp_reset_frame(consume_fr);

		consume_fr->cmd = "SUBSCRIBE";
		cstmp_add_header(consume_fr, "destination", QUEUE_NAME);
		cstmp_add_header(consume_fr, "ack", "auto");
		cstmp_add_header(consume_fr, "id", "0");
		cstmp_add_header(consume_fr, "durable", "false");
		cstmp_send(consuming_sess, consume_fr, 3);

		void (*consume_handler)(cstmp_frame_t* consume_fr) =
		({
			void __fn__ (cstmp_frame_t* consume_fr) {
				assert( strlen(cstmp_get_cmd(consume_fr)) > 0 && "Error, no command found");
				cstmp_dump_frame_pretty(consume_fr);
			}
			__fn__;
		});

		/** Keep hooking until consuming = false / 0 **/
		cstmp_consume(consuming_sess, (cstmp_frame_t*) consume_fr, consume_handler, &consuming);

		/** you can use direct string if you don't want frame to send **/
		if (cstmp_send_direct(consuming_sess, "UNSUBSCRIBE\nid:0\n\n", 0) &&
		        cstmp_recv(consuming_sess, consume_fr, 0) ) {
			cstmp_dump_frame_pretty(consume_fr);
		}


		consume_fr->cmd = "DISCONNECT";
		cstmp_add_header(consume_fr, "receipt", "dummy-recv-test");
		if ( cstmp_send(consuming_sess, consume_fr, 0) &&
		        cstmp_recv(consuming_sess, consume_fr, 0) ) {
			cstmp_dump_frame_pretty(consume_fr);
		}

		cstmp_destroy_frame(consume_fr);
		cstmp_disconnect(consuming_sess);
	} else
		printf("%s\n", "consuming Failed");

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
	if (sess) {
		cstmp_frame_t *fr = cstmp_new_frame(sess);
		if (fr) {
			fr->cmd = "CONNECT";
			cstmp_add_header_str(fr, "version:1.2"); // for direct string set method
			cstmp_add_header(fr, "login", "guest"); // for key val set method
			cstmp_add_header_str_and_len(fr, "passcode:guest", sizeof("passcode:guest") - 1); // in case you need len specified

			cstmp_frame_val_t val;

			if (cstmp_send(sess, fr, 0) && cstmp_recv(sess, fr, 0)) {
				cstmp_dump_frame_pretty(fr);
			}

			/*** Send 100 times ***/
			int send_count = 100;
			while (send_count--) {
				cstmp_reset_frame(fr);
				fr->cmd = "SEND";
				cstmp_add_header(fr, "destination", QUEUE_NAME);
				cstmp_add_header(fr, "persistent", "false");
				cstmp_add_header(fr, "content-type", "text/plain");
				cstmp_add_body_content(fr, "{\"my_key\":\"akjdlkajdklj2ljladasjldjasljdl@ASD2\"}");

				if (! cstmp_send(sess, fr, 1)) {
					assert(0 && "Unable to send frame");
					break;
				}
				usleep(1000 * 50);
			}

			consuming = 0;
			printf("%s\n", "disconnecting Stomp");
			usleep(1000 * 3000);

			fr->cmd = "DISCONNECT";
			cstmp_add_header(fr, "receipt", "dummy-send-test");
			if ( cstmp_send(sess, fr, 3) && cstmp_recv(sess, fr, 3) ) {
				cstmp_dump_frame_pretty(fr);
			}


			cstmp_destroy_frame(fr);
		} else {
			printf("%s\n", "Test Failed");
			return 0;
		}
		cstmp_disconnect(sess);
		printf("%s\n", "Test Passed");
	} else
		printf("%s\n", "Test Failed");


	return 0;
}