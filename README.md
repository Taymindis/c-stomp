c-stomp
=======

A STOMP client written in c, [STOMP](https://stomp.github.io/) is the Simple (or Streaming) Text Orientated Messaging Protocol.

Table of Contents
=================

* [Introduction](#introduction)
* [Example](#example)
* [Installation](#installation)
* [Uninstall](#uninstall)
* [Tips And Tricks](#tips-and-tricks)
* [Support](#support)
* [Copyright & License](#copyright--license)

Introduction
============

c-stomp is a C library to write STOMP protocol in order to working with ActiveMQ, RabbitMQ, HornetQ, ActiveMQ Apollo and other messaging protocol which support stomp.

Example
======
```c
    /*For Connecting to Stomp */
    cstmp_session_t *sess = cstmp_connect(HOST, PORT);
    cstmp_frame_t *fr = cstmp_create_frame(sess);
    fr->cmd = "CONNECT";
    cstmp_add_header(fr, "login", "guest");
    cstmp_add_header(fr, "passcode", "guest");
    if (cstmp_send(fr, 1000/*timeout_ms*/, 0/*conn retry time*/) && cstmp_recv(fr, 1000/*timeout_ms*/, 0/*conn retry time*/)) {
        cstmp_dump_frame_pretty(fr); /*For Display purpose*/
    }
```
```c
    /* For Sending to */
    cstmp_reset_frame(fr); // Remember to reset frame for every send command.
    fr->cmd = "SEND";
    cstmp_add_header(fr, "destination", QUEUE_NAME);
    cstmp_add_header(fr, "persistent", "false");
    cstmp_add_header(fr, "content-type", "text/plain");
    cstmp_add_body_content(fr, "{\"my_key\":\"akjdlkajdklj2ljladasjldjasljdl@ASD2\"}");

    if (! cstmp_send(fr, 1000, 1)) {
        assert(0 && "Unable to send frame");
    }
```
```c
    /* For Reading Response */
    if(cstmp_recv(fr, 1000, 0)) {
        cstmp_dump_frame_raw(fr); /*For Raw message display, good for debugging purpose*/ 
    }
```
```c

    /** To create a hooker to consume the message and callback handler **/

    /*** remember to reset frame before prepare the new command ***/
    cstmp_reset_frame(consume_fr);

    consume_fr->cmd = "SUBSCRIBE";
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
    cstmp_consume((cstmp_frame_t*) consume_fr, consume_handler, &consuming, 1000/*Reloop time, can ignore*/);
```
[Back to TOC](#table-of-contents)


Installation
============

```bash
cd $project_root_dir
mkdir build
cd build
cmake ..
make -j2
./run-test
sudo make install
```
[Back to TOC](#table-of-contents)

Uninstall
=========
```bash
cd $project_root_dir/build
sudo make uninstall
```


Tips And Tricks
===============

If you are sharing send and receive frame with the same session(connection/socket), you should create frame with it's role, so the frame know.

For Read only frame and Send only frame, they are sharing same connection, but one is only for send out, another only for read in.
```c
cstmp_frame_t *consume_frame = cstmp_create_frame_r(sess, cstmp_read_only_frame);
cstmp_frame_t *send_frame = cstmp_create_frame_r(sess, cstmp_write_only_frame);
```

If you know want to make frame read and writable, just create as normal, but this 2 frame might send and receive different response, enjoy and be safe.
```c
cstmp_frame_t *fr1 = cstmp_create_frame(sess);
cstmp_frame_t *fr2 = cstmp_create_frame(sess);
```

[Back to TOC](#table-of-contents)

Support
=======

Please do not hesitate to contact minikawoon2017@gmail.com/minikawoon99@gmail.com for any queries.


[Back to TOC](#table-of-contents)

Copyright & License
===================

MIT License

Copyright (c) 2018, Taymindis Woon <cloudleware2015@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
