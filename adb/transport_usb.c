/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sysdeps.h>

#define  TRACE_TAG  TRACE_TRANSPORT
#include "adb.h"

/* XXX better define? */
#ifdef __ppc__
#define H4(x)	(((x) & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | (((x) & 0x000000FF) << 24)
static inline void fix_endians(apacket *p)
{
    p->msg.command     = H4(p->msg.command);
    p->msg.arg0        = H4(p->msg.arg0);
    p->msg.arg1        = H4(p->msg.arg1);
    p->msg.data_length = H4(p->msg.data_length);
    p->msg.data_check  = H4(p->msg.data_check);
    p->msg.magic       = H4(p->msg.magic);
}
unsigned host_to_le32(unsigned n)
{
    return H4(n);
}
#else
#define fix_endians(p) do {} while (0)
unsigned host_to_le32(unsigned n)
{
    return n;
}
#endif

static int remote_read(apacket *p, atransport *t)
{
    if(usb_read(t->usb, &p->msg, sizeof(amessage))){
        D("remote usb: read terminated (message)\n");
        return -1;
    }

    fix_endians(p);

    if(check_header(p)) {
        D("remote usb: check_header failed\n");
        return -1;
    }

    if(p->msg.data_length) {
        if(usb_read(t->usb, p->data, p->msg.data_length)){
            D("remote usb: terminated (data)\n");
            return -1;
        }
    }

    if(check_data(p)) {
        D("remote usb: check_data failed\n");
        return -1;
    }

    return 0;
}

static int remote_write(apacket *p, atransport *t)
{
    unsigned size = p->msg.data_length;

    fix_endians(p);

    if(usb_write(t->usb, &p->msg, sizeof(amessage))) {
        D("remote usb: 1 - write terminated\n");
        return -1;
    }
    if(p->msg.data_length == 0) return 0;
    if(usb_write(t->usb, &p->data, size)) {
        D("remote usb: 2 - write terminated\n");
        return -1;
    }

    return 0;
}

static void remote_close(atransport *t)
{
    usb_close(t->usb);
    t->usb = 0;
}

static void remote_kick(atransport *t)
{
    usb_kick(t->usb);
}

void init_usb_transport(atransport *t, usb_handle *h)
{
    D("transport: usb\n");
    t->close = remote_close;
    t->kick = remote_kick;
    t->read_from_remote = remote_read;
    t->write_to_remote = remote_write;
    t->sync_token = 1;
    t->connection_state = CS_OFFLINE;
    t->type = kTransportUsb;
    t->usb = h;

#if ADB_HOST
    HOST = 1;
#else
    HOST = 0;
#endif
}

int is_adb_interface(int vid, int pid, int usb_class, int usb_subclass, int usb_protocol)
{
    if (vid == VENDOR_ID_GOOGLE) {
            /* might support adb */
    } else if (vid == VENDOR_ID_HTC) {
            /* might support adb */
    } else {
            /* not supported */
        return 0;
    }

        /* class:vendor (0xff) subclass:android (0x42) proto:adb (0x01) */
    if(usb_class == 0xff) {
        if((usb_subclass == 0x42) && (usb_protocol == 0x01)) {
            return 1;
        }
    }

    return 0;
}
