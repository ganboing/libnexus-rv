// SPDX-License-Identifer: Apache 2.0
/*
 * replay.c - Trace replayer utility that drives trace decoder
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdlib.h>
#include <inttypes.h>
#include <getopt.h>
#include <error.h>
#include <assert.h>
#include <fcntl.h>
#include <libnexus-rv/error.h>
#include <libnexus-rv/msg-decoder.h>
#include <libnexus-rv/trace-decoder.h>
#include "opts-def.h"
#include "misc.h"

#define DEFAULT_BUFFER_SIZE 4096

static struct option long_opts[] = {
        {"help",      no_argument,       NULL, 'h'},
        {"tsbits",    required_argument, NULL, 't'},
        {"srcbits",   required_argument, NULL, 's'},
        {"buffersz",  required_argument, NULL, 'b'},
        {NULL, 0,                        NULL, 0},
};

static const char short_opts[] = "hb:t:s:";

static void help(const char *argv0) {
    error(-1, 0, "Usage: \n"
                  "\t%s: [OPTIONS...] <trace file> or - for stdin\n"
                  "\n"
                  "\t-h, --help       Display this help message\n"
                  "\t-t, --tsbits     Bits of Timestamp, default 0\n"
                  "\t-s, --srcbits    Bits of SRC field, default 0\n"
                  "\t-b, --buffersz   Buffer size (default %d)\n",
          argv0, DEFAULT_BUFFER_SIZE);
}

#define FMT_TIME_OFFSET "\n[%" PRIu64 "] +%zu "

static void replay(nexusrv_hw_cfg *hwcfg, FILE *fp, int fd, size_t bufsz) {
    void *buffer = malloc(bufsz);
    if (!buffer)
        error(-1, 0, "Failed to allocate buffer");
    nexusrv_msg_decoder msg_decoder = {};
    nexusrv_msg_decoder_init(&msg_decoder, hwcfg, fd, buffer, bufsz);
    nexusrv_trace_decoder trace_decoder = {};
    int32_t rc = nexusrv_trace_decoder_init(&trace_decoder, &msg_decoder);
    if (rc < 0)
        error(-rc, 0, "decoder_init failed: %s",
              str_nexus_error(-rc));
    uint64_t tnt_time = 0;
    uint64_t last_time = 0;
    for (;;) {
        nexusrv_msg msg;
        nexusrv_trace_indirect indir;
        nexusrv_trace_sync sync;
        nexusrv_trace_stop stop;
        nexusrv_trace_error err;
        unsigned event = NEXUSRV_Trace_Event_Sync;
        rc = nexusrv_trace_sync_reset(&trace_decoder, &sync);
        if (rc < 0)
            error(-rc, 0, "sync_reset failed: %s",
                  str_nexus_error(-rc));
        if (rc > 0) {
            fprintf(fp, FMT_TIME_OFFSET "SYNC %" PRIx64 " sync=%u",
                    nexusrv_trace_time(&trace_decoder),
                    nexusrv_msg_decoder_offset(&msg_decoder),
                    sync.addr << 1, sync.sync);
            goto check_time;
        }
        rc = nexusrv_trace_try_retire(&trace_decoder, UINT32_MAX, &event);
        if (rc < 0) switch (rc) {
            case -nexus_trace_eof:
                goto done_trace;
            case -nexus_msg_unsupported:
                goto unknown_msg;
            default:
                error(-rc, 0, "try_retire failed: %s",
                      str_nexus_error(-rc));
        }
        if (rc) {
            tnt_time = 0;
            fprintf(fp, FMT_TIME_OFFSET "I-CNT %" PRIi32,
                    nexusrv_trace_time(&trace_decoder),
                    nexusrv_msg_decoder_offset(&msg_decoder),
                    rc);
        }
        switch (event) {
            case NEXUSRV_Trace_Event_Direct: {
                rc = nexusrv_trace_next_tnt(&trace_decoder);
                if (rc < 0)
                    error(-rc, 0, "next_tnt failed: %s",
                          str_nexus_error(-rc));
                if (tnt_time != nexusrv_trace_time(&trace_decoder))
                    fprintf(fp, FMT_TIME_OFFSET "TNT ",
                            nexusrv_trace_time(&trace_decoder),
                            nexusrv_msg_decoder_offset(&msg_decoder));
                fprintf(fp, "%c", rc > 0 ? '!' : '.');
                tnt_time = nexusrv_trace_time(&trace_decoder);
                break;
            }
            case NEXUSRV_Trace_Event_Indirect: {
                rc = nexusrv_trace_next_indirect(&trace_decoder, &indir);
                if (rc < 0)
                    error(-rc, 0, "next_indir failed: %s",
                          str_nexus_error(-rc));
                fprintf(fp, FMT_TIME_OFFSET "INDIRECT %" PRIx64 "%s%s",
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(&msg_decoder),
                        indir.target << 1,
                        indir.interrupt ? " interrupt" : "",
                        indir.exception ? " exception" : "");
                if (indir.ownership)
                    fprintf(fp, " fmt=%u priv=%u v=%u context=%" PRIx64,
                            indir.ownership_fmt,
                            indir.ownership_priv,
                            indir.ownership_v,
                            indir.context);
                break;
            }
            case NEXUSRV_Trace_Event_Sync: {
                rc = nexusrv_trace_next_sync(&trace_decoder, &sync);
                if (rc < 0)
                    error(-rc, 0, "next_sync failed: %s",
                          str_nexus_error(-rc));
                fprintf(fp, FMT_TIME_OFFSET "SYNC %" PRIx64 " sync=%u",
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(&msg_decoder),
                        sync.addr << 1, sync.sync);
                break;
            }
            case NEXUSRV_Trace_Event_Stop: {
                rc = nexusrv_trace_next_stop(&trace_decoder, &stop);
                if (rc < 0)
                    error(-rc, 0, "next_stop failed: %s",
                          str_nexus_error(-rc));
                fprintf(fp, FMT_TIME_OFFSET "STOP evcode=%u",
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(&msg_decoder),
                        stop.evcode);
                break;
            }
            case NEXUSRV_Trace_Event_Error: {
                rc = nexusrv_trace_next_error(&trace_decoder, &err);
                if (rc < 0)
                    error(-rc, 0, "next_error failed: %s",
                          str_nexus_error(-rc));
                fprintf(fp, FMT_TIME_OFFSET "ERROR etype=%u ecode=%u",
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(&msg_decoder),
                        err.etype, err.ecode);
                break;
            }
        }
check_time:
        if (last_time && last_time > nexusrv_trace_time(&trace_decoder)) {
            error(0, 0, "WARN: Time goes backward, %" PRIu64 " vs %"PRIu64,
                  last_time, nexusrv_trace_time(&trace_decoder));
        }
        last_time = nexusrv_trace_time(&trace_decoder);
        continue;
unknown_msg:
        rc = nexusrv_msg_decoder_next(&msg_decoder, &msg);
        if (rc < 0)
            error(-rc, 0, "msg_decoder_next failed: %s",
                  str_nexus_error(-rc));
        assert(rc > 0);
        fprintf(fp, "\n[%" PRIu64 "] UNKNOWN MSG ", nexusrv_trace_time(&trace_decoder));
        nexusrv_print_msg(fp, &msg);
    }
done_trace:
    nexusrv_trace_decoder_fini(&trace_decoder);
    free(buffer);
}

int main(int argc, char **argv) {
    nexusrv_hw_cfg hwcfg = {};
    hwcfg.ext_sifive = true; // Enable Sifive extension by default
    size_t bufsz = DEFAULT_BUFFER_SIZE;
    OPT_PARSE_BEGIN
    OPT_PARSE_H_HELP
    OPT_PARSE_T_TSBITS
    OPT_PARSE_S_SRCBITS
    OPT_PARSE_B_BUFSZ
    OPT_PARSE_END
    if (argc == optind)
        error(-1, 0, "Insufficient arguments");
    char *filename = argv[optind];
    int fd = open_seek_file(filename, O_RDONLY);
    replay(&hwcfg, stdout, fd, bufsz);
    close(fd);
    return 0;
}