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
#include <libnexus-rv/msg-encoder.h>
#include "opts-def.h"
#include "misc.h"

static struct option long_opts[] = {
        {"help",      no_argument,       NULL, 'h'},
        {"text",      no_argument,       NULL, 'x'},
        {"tsbits",    required_argument, NULL, 't'},
        {"srcbits",   required_argument, NULL, 's'},
        {NULL, 0,                        NULL, 0},
};

static const char short_opts[] = "hxts:";

static void help(const char *argv0) {
    error(-1, 0, "Usage: \n"
                "\t%s: [OPTIONS...] [<output trace file> or stdout if not specified] \n"
                "\n"
                "\t-h, --help       Display this help message\n"
                "\t-t, --tsbits     Bits of Timestamp, default 0\n"
                "\t-s, --srcbits    Bits of SRC field, default 0\n"
                "\t-x, --text       Text mode\n",
          argv0);
}

static void assemble(nexusrv_hw_cfg *hwcfg, FILE *fp, bool text) {
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
            error(-1, 0, "Unexpected msg prefix");
        free(type);
        rc = nexusrv_read_msg(stdin, &msg);
        if (rc < 0)
            error(-rc, 0, "Failed to parse msg: %d", (int)rc);
        memset(buffer, 0, NEXUS_RV_MSG_MAX_BYTES);
        ssize_t bytes = nexusrv_msg_encode(hwcfg, buffer, NEXUS_RV_MSG_MAX_BYTES, &msg);
        if (bytes < 0)
            error(-rc, 0, "Failed to encode msg: %d", (int)rc);
        emitted += bytes;
        if (text) {
            fprintf(fp, "[%zu]", bytes);
            for (unsigned i = 0; i < bytes; ++i)
                fprintf(fp, " %02hhx", buffer[i]);
            fputc('\n', fp);
        } else if (fwrite(buffer, bytes, 1, fp) != 1)
            error(-1, errno, "Failed to write msg");
    }
    fprintf(stderr, "\n Last Msg %zu, Emitted %zu bytes\n", msgid, emitted);
}

int main(int argc, char **argv) {
    nexusrv_hw_cfg hwcfg = {};
    bool text = false;
    OPT_PARSE_BEGIN
    OPT_PARSE_H_HELP
    OPT_PARSE_T_TSBITS
    OPT_PARSE_S_SRCBITS
    OPT_PARSE_X_TEXT
    OPT_PARSE_END
    FILE *fp = stdout;
    if (argc != optind)
        fp = fopen(argv[optind], "wb");
    else
        fp = fdopen(STDOUT_FILENO, "wb");
    if (!fp)
        error(-1, errno, "Failed to open output");
    if (isatty(fileno(fp))) {
        error(-1, 0, "Output to tty, forcing text mode");
        text = true;
    }
    assemble(&hwcfg, fp, text);
}