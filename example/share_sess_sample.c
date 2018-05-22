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

static cstmp_session_t *sess;
void *consuming_thread(void* none) {
	if (sess) {
		/** This session already subscribe(ref line 64), so we just create a readonly frame. **/
		cstmp_frame_t *consume_fr = cstmp_create_frame_r(sess, cstmp_read_only_frame);
		if (consume_fr) {
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

			cstmp_destroy_frame(consume_fr);

		}
	} else
		printf("%s\n", "consuming Failed");

	pthread_exit(NULL);
}

int main() {

	printf("%s %s\n", "Starting stomp connection, sending and consuming", QUEUE_NAME);
	usleep(1000 * 3000);

	sess = cstmp_connect(HOST, PORT);

	/** This frame are readable and writable**/
	cstmp_frame_t *fr = cstmp_create_frame(sess);

	/** since fr and consume_fr are using same session **/
	fr->cmd = "CONNECT";
	cstmp_add_header_str(fr, "version:1.2"); // for direct string set method
	cstmp_add_header(fr, "login", "guest"); // for key val set method
	cstmp_add_header_str_and_len(fr, "passcode:guest", sizeof("passcode:guest") - 1); // in case you need len specified

	if (cstmp_send(fr, 1000, 0) && cstmp_recv(fr, 1000, 0)) {
		cstmp_dump_frame_pretty(fr);
	}

	/*** remember to reset frame before prepare the new command ***/
	cstmp_reset_frame(fr);

	fr->cmd = "SUBSCRIBE";
	cstmp_add_header(fr, "destination", QUEUE_NAME);
	cstmp_add_header(fr, "ack", "auto");
	cstmp_add_header(fr, "id", "0");
	cstmp_add_header(fr, "durable", "false");
	cstmp_send(fr, 1000, 3);

	/** Create a thread for consuming **/
	pthread_t t;
	if (pthread_create(&t, NULL, consuming_thread, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}
	pthread_detach(t);

	/*** Sending ***/
	if (sess) {

		if (fr) {
			/*** Send 100 times ***/
			int send_count = 100;
			while (send_count--) {
				cstmp_reset_frame(fr);
				fr->cmd = "SEND";
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

			fr->cmd = "DISCONNECT";
			cstmp_add_header(fr, "receipt", "dummy-send-consume-test");
			if ( cstmp_send(fr, 1000, 3) && cstmp_recv(fr, 1000, 3) ) {
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