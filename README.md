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
    cstmp_session_t *consuming_sess = cstmp_connect_t(HOST, PORT, 500/*send_timeout*/, 500/*recv_timeout*/);
    if (consuming_sess) {
        cstmp_frame_t *consume_fr = cstmp_new_frame();
        consume_fr->cmd = "CONNECT";
        cstmp_add_header(consume_fr, "login", "guest");
        cstmp_add_header(consume_fr, "passcode", "guest");
        cstmp_add_header(consume_fr, "version", "1.2");

        if (cstmp_send(consuming_sess, consume_fr, 0) && cstmp_recv(consuming_sess, consume_fr, 0)) {
            cstmp_dump_frame_pretty(consume_fr);
        }
    }
```
```c
    /* For Sending to */
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
```
```c
    /* For Reading Response */
    if(cstmp_recv(sess, fr, 2 /*conn retry 2 times*/)) {
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
