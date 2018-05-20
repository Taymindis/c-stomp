c-stomp
=======

A STOMP client written in c, [STOMP](https://stomp.github.io/) is the Simple (or Streaming) Text Orientated Messaging Protocol.

Table of Contents
=================

* [Introduction](#introduction)
* [Example](#example)
* [Installation](#installation)
* [Uninstall](#uninstall)
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
    fr->cmd = CONNECT;
    cstmp_add_header(fr, "login", "guest");
    cstmp_add_header(fr, "passcode", "guest");
    if (cstmp_send(fr, 1000/*timeout_ms*/, 0/*conn retry time*/) && cstmp_recv(fr, 1000/*timeout_ms*/, 0/*conn retry time*/)) {
        cstmp_dump_frame_pretty(fr); /*For Display purpose*/
    }
```
```c
    /* For Sending to */
    cstmp_reset_frame(fr); // Remember to reset frame for every send command.
    fr->cmd = SEND;
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

[Back to TOC](#table-of-contents)

Support
=======

Please do not hesitate to contact minikawoon2017@gmail.com/minikawoon99@gmail.com for any queries.


[Back to TOC](#table-of-contents)

Copyright & License
===================

Copyright (c) 2018, Taymindis <cloudleware2015@gmail.com>

This module is licensed under the terms of the BSD license.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

[Back to TOC](#table-of-contents)