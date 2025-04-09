// SPDX-License-Identifer: Apache 2.0
/*
 * assumble.c - Message assembler utility that drives encoder.
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <libnexus-rv/encoder.h>
#include "misc.h"

static struct option long_opts[] = {
        {"help",      no_argument,       NULL, 'h'},
        {"text",      no_argument,       NULL, 'x'},
        {"timestamp", no_argument,       NULL, 't'},
        {"srcbits",   required_argument, NULL, 's'},
        {NULL, 0,                        NULL, 0},
};

static const char short_opts[] = "hxts:";

static void help(const char *argv0) {
    error(1, 0, "Usage: \n"
                "\t%s: [OPTIONS...] [<output trace file> or stdout if not specified] \n"
                "\n"
                "\t-h, --help       Display this help message\n"
                "\t-t, --timestamp  Enable timestamp\n"
                "\t-s, --srcbits    Bits of SRC field, default 0\n"
                "\t-x, --text       Text mode\n",
          argv0);
}

static ssize_t assemble(nexusrv_dencoder_cfg *cfg, FILE *fp, bool text) {
    size_t msgid = 0;
    size_t emitted = 0;
    uint8_t buffer[NEXUS_RV_MSG_MAX_BYTES];
    for (;;) {
        nexusrv_msg msg;
        char *type;
        int rc = scanf(" Msg #%zu %ms", &msgid, &type);
        if (rc == EOF)
            break;
        if (rc < 2)
            error(1, 0, "Unexpected msg prefix");
        free(type);
        rc = nexusrv_read_msg(stdin, &msg);
        if (rc < 0)
            error(1, 0, "Failed to parse msg: %d", rc);
        memset(buffer, 0, NEXUS_RV_MSG_MAX_BYTES);
        ssize_t bytes = nexusrv_msg_encode(cfg, buffer, NEXUS_RV_MSG_MAX_BYTES, &msg);
        if (bytes < 0)
            error(1, 0, "Failed to encode msg");
        emitted += bytes;
        if (text) {
            fprintf(fp, "[%zu]", bytes);
            for (unsigned i = 0; i < bytes; ++i)
                fprintf(fp, " %02hhx", buffer[i]);
            fputc('\n', fp);
        } else if (fwrite(buffer, bytes, 1, fp) != 1)
            error(1, errno, "Failed to write msg");
    }
    fprintf(stderr, "\n Last Msg %zu, Emitted %zu bytes\n", msgid, emitted);
}

int main(int argc, char **argv) {
    int optidx = 0;
    int optchar;
    nexusrv_dencoder_cfg decoder_cfg = {};
    bool text = false;
    while ((optchar = getopt_long(
            argc, argv,
            short_opts, long_opts, &optidx)) != -1) {
        switch (optchar) {
            case 'h':
                help(argv[0]);
                return 1;
            case 't':
                decoder_cfg.has_timestamp = true;
                break;
            case 's':
                decoder_cfg.src_bits = atoi(optarg);
                break;
            case 'x':
                text = true;
                break;
            default:
                return 1;
        }
    }
    FILE *fp = stdout;
    if (argc != optind)
        fp = fopen(argv[optind], "wb");
    else
        fp = fdopen(STDOUT_FILENO, "wb");
    if (!fp)
        error(1, errno, "Failed to open output");
    if (isatty(fileno(fp))) {
        error(0, 0, "Output to tty, forcing text mode");
        text = true;
    }
    assemble(&decoder_cfg, fp, text);
}