// SPDX-License-Identifer: Apache 2.0
/*
 * dump.c - Message dumper utility that drives decoder.
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <error.h>
#include <string.h>
#include <fcntl.h>
#include <libnexus-rv/msg-decoder.h>
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

static void dump(nexusrv_hw_cfg *hwcfg, FILE *fp, int fd, size_t bufsz) {
    size_t msgid = 0;
    void *buffer = malloc(bufsz);
    if (!buffer)
        error(-1, 0, "Failed to allocate buffer");
    nexusrv_msg_decoder msg_decoder = {};
    nexusrv_msg_decoder_init(&msg_decoder, hwcfg, fd, buffer, bufsz);
    size_t decoded_bytes = 0;
    ssize_t rc;
    for (;; decoded_bytes += rc, ++msgid) {
        nexusrv_msg msg;
        rc = nexusrv_msg_decoder_next(&msg_decoder, &msg);
        if (rc < 0)
            error(-rc, 0, "Failed to decode msg: %d", (int)rc);
        if (!rc)
            break;
        fprintf(fp, "Msg #%zu +%zu ", msgid, decoded_bytes);
        nexusrv_print_msg(fp, &msg);
        fputc('\n', fp);
    }
    fflush(fp);
    free(buffer);
    fprintf(stderr, "\n Total: %zu Msg, Decoded %zu bytes\n",
            msgid, decoded_bytes);
}

int main(int argc, char **argv) {
    nexusrv_hw_cfg hwcfg = {};
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
    dump(&hwcfg, stdout, fd, bufsz);
    close(fd);
    return 0;
}