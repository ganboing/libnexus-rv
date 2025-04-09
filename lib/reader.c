// SPDX-License-Identifer: Apache 2.0
/*
 * reader.c - Nexus-RV message reader
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdio.h>
#include <libnexus-rv/error.h>
#include <libnexus-rv/decoder.h>
#include "format.h"

#define CHECK_SCANF(...)                    \
do {                                        \
    int rc = fscanf(fp, __VA_ARGS__);       \
    if (rc <= 0)                            \
        return -nexus_msg_missing_field;    \
} while(0)

int nexusrv_read_msg(FILE *fp, nexusrv_msg *msg) {
    unsigned src, tcode;
    uint64_t timestamp;
    CHECK_SCANF(NEXUS_FMT_SRC, &src);
    CHECK_SCANF(NEXUS_FMT_TIMESTAMP, &timestamp);
    CHECK_SCANF(NEXUS_FMT_TCODE, &tcode);
    msg->common.source = src;
    msg->common.timestamp = timestamp;
    msg->common.tcode = tcode;
    switch (msg->common.tcode) {
        default:
            return -nexus_msg_unsupported;
        case NENUSRV_TCODE_OWNERSHIP: {
            unsigned format, prv, v;
            uint64_t context;
            CHECK_SCANF(NEXUS_FMT_OWNER_FORMAT, &format);
            CHECK_SCANF(NEXUS_FMT_OWNER_PRV, &prv);
            CHECK_SCANF(NEXUS_FMT_OWNER_V, &v);
            CHECK_SCANF(NEXUS_FMT_OWNER_CONTEXT, &context);
            msg->ownership.format = format;
            msg->ownership.prv = prv;
            msg->ownership.v = v;
            msg->ownership.context = context;
            break;
        }
        case NEXUSRV_TCODE_ERROR: {
            unsigned etype;
            uint32_t ecode;
            CHECK_SCANF(NEXUS_FMT_ERROR_ETYPE, &etype);
            CHECK_SCANF(NEXUS_FMT_ERROR_ECODE, &ecode);
            msg->error.etype = etype;
            msg->error.ecode = ecode;
            break;
        }
        case NEXUSRV_TCODE_DIRBR: {
            uint32_t icnt;
            CHECK_SCANF(NEXUS_FMT_ICNT, &icnt);
            msg->dirbr.icnt = icnt;
            break;
        }
        case NEXUSRV_TCODE_INDIRBR: {
            unsigned btype;
            uint32_t icnt;
            uint64_t uaddr;
            CHECK_SCANF(NEXUS_FMT_ICNT, &icnt);
            CHECK_SCANF(NEXUS_FMT_BTYPE, &btype);
            CHECK_SCANF(NEXUS_FMT_UADDR, &uaddr);
            msg->indirbr.icnt = icnt;
            msg->indirbr.btype = btype;
            msg->indirbr.uaddr = uaddr;
            break;
        }
        case NEXUSRV_TCODE_PROG_SYNC: {
            unsigned sync;
            uint32_t icnt;
            uint64_t faddr;
            CHECK_SCANF(NEXUS_FMT_ICNT, &icnt);
            CHECK_SCANF(NEXUS_FMT_SYNC, &sync);
            CHECK_SCANF(NEXUS_FMT_FADDR, &faddr);
            msg->prog_sync.icnt = icnt;
            msg->prog_sync.sync = sync;
            msg->prog_sync.faddr = faddr;
            break;
        }
        case NEXUSRV_TCODE_DIRBR_SYNC: {
            unsigned sync;
            uint32_t icnt;
            uint64_t faddr;
            CHECK_SCANF(NEXUS_FMT_ICNT, &icnt);
            CHECK_SCANF(NEXUS_FMT_SYNC, &sync);
            CHECK_SCANF(NEXUS_FMT_FADDR, &faddr);
            msg->dirbr_sync.icnt = icnt;
            msg->dirbr_sync.sync = sync;
            msg->dirbr_sync.faddr = faddr;
            break;
        }
        case NEXUSRV_TCODE_INDIRBR_SYNC: {
            unsigned sync, btype;
            uint32_t icnt;
            uint64_t faddr;
            CHECK_SCANF(NEXUS_FMT_ICNT, &icnt);
            CHECK_SCANF(NEXUS_FMT_SYNC, &sync);
            CHECK_SCANF(NEXUS_FMT_BTYPE, &btype);
            CHECK_SCANF(NEXUS_FMT_FADDR, &faddr);
            msg->indirbr_sync.icnt = icnt;
            msg->indirbr_sync.sync = sync;
            msg->indirbr_sync.btype = btype;
            msg->indirbr_sync.faddr = faddr;
            break;
        }
        case NEXUSRV_TCODE_INDIRBR_HIST: {
            unsigned btype;
            uint32_t icnt, hist;
            uint64_t uaddr;
            CHECK_SCANF(NEXUS_FMT_ICNT, &icnt);
            CHECK_SCANF(NEXUS_FMT_BTYPE, &btype);
            CHECK_SCANF(NEXUS_FMT_UADDR, &uaddr);
            CHECK_SCANF(NEXUS_FMT_HIST, &hist);
            msg->indirbr_hist.icnt = icnt;
            msg->indirbr_hist.btype = btype;
            msg->indirbr_hist.uaddr = uaddr;
            msg->indirbr_hist.hist = hist;
            break;
        }
        case NEXUSRV_TCODE_INDIRBR_HIST_SYNC: {
            unsigned btype, sync;
            uint32_t icnt, hist;
            uint64_t faddr;
            CHECK_SCANF(NEXUS_FMT_ICNT, &icnt);
            CHECK_SCANF(NEXUS_FMT_SYNC, &sync);
            CHECK_SCANF(NEXUS_FMT_BTYPE, &btype);
            CHECK_SCANF(NEXUS_FMT_FADDR, &faddr);
            CHECK_SCANF(NEXUS_FMT_HIST, &hist);
            msg->indirbr_hist_sync.icnt = icnt;
            msg->indirbr_hist_sync.sync = sync;
            msg->indirbr_hist_sync.btype = btype;
            msg->indirbr_hist_sync.faddr = faddr;
            msg->indirbr_hist_sync.hist = hist;
            break;
        }
        case NEXUSRV_TCODE_RESOURCE_FULL: {
            unsigned rcode;
            CHECK_SCANF(NEXUS_FMT_RCODE, &rcode);
            msg->resource_full.rcode = rcode;
            switch (msg->resource_full.rcode) {
                case 0: {
                    uint32_t icnt;
                    CHECK_SCANF(NEXUS_FMT_ICNT, &icnt);
                    msg->resource_full.icnt = icnt;
                    break;
                }
                case 1: {
                    uint32_t hist;
                    CHECK_SCANF(NEXUS_FMT_HIST, &hist);
                    msg->resource_full.hist = hist;
                    break;
                }
                case 2: {
                    uint32_t hist, hrepeat;
                    CHECK_SCANF(NEXUS_FMT_HIST, &hist);
                    CHECK_SCANF(NEXUS_FMT_HREPEAT, &hrepeat);
                    msg->resource_full.hist = hist;
                    msg->resource_full.hrepeat = hrepeat;
                    break;
                }
                default: {
                    uint32_t rdata;
                    CHECK_SCANF(NEXUS_FMT_RDATA, &rdata);
                    msg->resource_full.rdata = rdata;
                    break;
                }
            }
            break;
        }
        case NEXUSRV_TCODE_REPEAT_BR: {
            uint32_t hrepeat;
            CHECK_SCANF(NEXUS_FMT_HREPEAT, &hrepeat);
            msg->repeat_br.hrepeat = hrepeat;
            break;
        }
        case NEXUSRV_TCODE_PROG_CORR: {
            unsigned evcode;
            uint32_t icnt, hist;
            CHECK_SCANF(NEXUS_FMT_EVCODE, &evcode);
            CHECK_SCANF(NEXUS_FMT_ICNT, &icnt);
            CHECK_SCANF(NEXUS_FMT_HIST, &hist);
            msg->prog_corr.evcode = evcode;
            msg->prog_corr.icnt = icnt;
            msg->prog_corr.hist = hist;
            break;
        }
    }
}