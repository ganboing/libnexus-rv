// SPDX-License-Identifer: Apache 2.0
/*
 * split.c - Split messages into per-src file
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <error.h>
#include <errno.h>
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
        {"prefix",    required_argument, NULL, 'p'},
        {NULL, 0,                        NULL, 0},
};

static const char short_opts[] = "ht:s:b:p:";

static void help(const char *argv0) {
    error(-1, 0, "Usage: \n"
                  "\t%s: [OPTIONS...] <trace file> or - for stdin\n"
                  "\n"
                  "\t-h, --help       Display this help message\n"
                  "\t-t, --tsbits     Bits of Timestamp, default 0\n"
                  "\t-s, --srcbits    Bits of SRC field, default 0\n"
                  "\t-b, --buffersz   Buffer size (default %d)\n"
                  "\t-p, --prefix     Filename prefix\n",
          argv0, DEFAULT_BUFFER_SIZE);
}

static void split(nexusrv_hw_cfg *hwcfg, int fd, size_t bufsz, const char *prefix) {
    FILE *fp_array[1 << hwcfg->src_bits];
    size_t decoded_src[1 << hwcfg->src_bits];
    size_t msgid_src[1 << hwcfg->src_bits];
    memset(&fp_array, 0, sizeof(fp_array));
    memset(&decoded_src, 0, sizeof(decoded_src));
    memset(&msgid_src, 0, sizeof(msgid_src));
    size_t msgid = 0;
    void *buffer = malloc(bufsz);
    if (!buffer)
        error(-1, 0, "Failed to allocate buffer");
    nexusrv_msg_decoder msg_decoder = {};
    nexusrv_msg_decoder_init(&msg_decoder, hwcfg, fd, -1, buffer, bufsz);
    size_t decoded_bytes = 0;
    ssize_t rc;
    for (;; decoded_bytes += rc, ++msgid) {
        nexusrv_msg msg;
        rc = nexusrv_msg_decoder_next(&msg_decoder, &msg);
        if (rc < 0)
            error(-rc, 0, "Failed to decode msg: %d", (int)rc);
        if (!rc)
            break;
        if (!nexusrv_msg_known(&msg)) {
            error(0, 0, "Unknown Msg %zu at %zu, ignored", msgid, decoded_bytes);
            continue;
        }
        if (nexusrv_msg_idle(&msg))
            continue;
        decoded_src[msg.src] += rc;
        ++msgid_src[msg.src];
        if (!fp_array[msg.src]) {
            int str_len = snprintf(NULL, 0,"%s.%u", prefix, msg.src);
            char filename[str_len + 1];
            sprintf(filename, "%s.%u", prefix, msg.src);
            fp_array[msg.src] = fopen(filename, "wb");
            if (!fp_array[msg.src])
                error(-1, errno, "Unable to open %s", filename);
        }
        if (fwrite(nexusrv_msg_decoder_lastmsg(&msg_decoder),
                   rc, 1, fp_array[msg.src]) != 1)
            error(-1, errno, "Failed to write raw msg: %d", (int)rc);
    }
    free(buffer);
    fprintf(stderr, "\n Total: %zu Msg, Decoded %zu bytes\n",
            msgid, decoded_bytes);
    for (size_t i = 0; i < (1U << hwcfg->src_bits); ++i) {
        if (!fp_array[i])
            continue;
        fprintf(stderr, "  SRC %zu: %zu Msg, Decoded %zu bytes\n",
                i, msgid_src[i], decoded_src[i]);
        fclose(fp_array[i]);
    }
}

int main(int argc, char **argv) {
    nexusrv_hw_cfg hwcfg = {};
    size_t bufsz = DEFAULT_BUFFER_SIZE;
    const char *prefix = NULL;
    OPT_PARSE_BEGIN
    OPT_PARSE_H_HELP
    OPT_PARSE_T_TSBITS
    OPT_PARSE_S_SRCBITS
    OPT_PARSE_B_BUFSZ
    OPT_PARSE_P_PREFIX
    OPT_PARSE_END
    if (argc == optind)
        error(-1, 0, "Insufficient arguments");
    char *filename = argv[optind];
    int fd = open_seek_file(filename, O_RDONLY);
    if (!prefix) {
        if (fd == STDIN_FILENO)
            error(-1, 0, "Prefix must be specified when reading from stdin");
        prefix = filename;
    }
    split(&hwcfg, fd, bufsz, prefix);
    close(fd);
    return 0;
}