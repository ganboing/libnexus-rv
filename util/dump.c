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
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <libnexus-rv/decoder.h>
#include "misc.h"

#define DEFAULT_BUFFER_SIZE 4096

static struct option long_opts[] = {
        {"help",      no_argument,       NULL, 'h'},
        {"sync",      no_argument,       NULL, 'y'},
        {"timestamp", no_argument,       NULL, 't'},
        {"srcbits",   required_argument, NULL, 's'},
        {"buffersz",  required_argument, NULL, 'b'},
        {"raw",       no_argument,       NULL, 'r'},
        {NULL, 0,                        NULL, 0},
};

static const char short_opts[] = "hb:yts:r";

static void help(const char *argv0) {
    error(1, 0, "Usage: \n"
                    "\t%s: [OPTIONS...] <trace file> or - for stdin\n"
                    "\n"
                    "\t-h, --help       Display this help message\n"
                    "\t-y, --sync       Synchronize before decoding\n"
                    "\t-t, --timestamp  Enable timestamp\n"
                    "\t-s, --srcbits    Bits of SRC field, default 0\n"
                    "\t-b, --buffersz   Buffer size (default %d)\n"
                    "\t-r, --raw        Decode to raw binary\n",
                    argv0, DEFAULT_BUFFER_SIZE);
}

static void seek_file(int fd, size_t offset) {
    bool seekable = true;
    ssize_t seeked = lseek(fd, offset, SEEK_SET);
    if (seeked < 0) {
        if (errno == ESPIPE)
            seekable = false;
        else
            error(1, errno, "Failed to seek file");
    }
    // Take care of unseekable pipe file
    if (!seekable) {
        seeked = seek_pipe(fd, offset);
        if (seeked < 0)
            error(1, errno, "Failed to seek pipe");
        if (seeked != offset)
            error(1, 0, "Short read of pipe");
    }
}

static ssize_t dump(nexusrv_dencoder_cfg *cfg, FILE *fp, int fd, size_t bufsz, bool synced, bool raw) {
    size_t msgid = 0;
    nexusrv_decoder_context context = {};
    memcpy(&context.decoder_cfg, cfg, sizeof(*cfg));
    context.fd = fd;
    context.bufsz = bufsz;
    context.buffer = malloc(bufsz);
    context.synced = synced;
    if (!context.buffer)
        error(1, 0, "Failed to allocate buffer");
    size_t decoded_bytes = 0, stor_bytes = 0;
    ssize_t rc;
    for (;;) {
        nexusrv_msg msg;
        rc = nexusrv_decoder_iterate(&context, &msg);
        if (rc <= 0)
            break;
        decoded_bytes += rc;
        size_t msgsz = nexusrv_msg_sz(&msg);
        stor_bytes += msgsz;
        if (raw) {
            if (write(fileno(fp), &msg, msgsz) != msgsz)
                error(1, errno, "Failed to write raw msg");
        } else {
            fprintf(fp, "Msg #%zu ", msgid++);
            nexusrv_print_msg(fp, &msg);
            fputc('\n', fp);
        }
    }
    free(context.buffer);
    fprintf(stderr, "\n Total: %zu Msg, Decoded %zu bytes, Storage %zu bytes\n",
            msgid, decoded_bytes, stor_bytes);
    if (rc < 0)
        return rc;
    return msgid;
}

int main(int argc, char **argv) {
    int optidx = 0;
    int optchar;
    nexusrv_dencoder_cfg decoder_cfg = {};
    bool raw = false;
    bool synced = true;
    size_t file_offset = 0;
    size_t bufsz = DEFAULT_BUFFER_SIZE;
    while ((optchar = getopt_long(
            argc, argv,
            short_opts, long_opts, &optidx)) != -1) {
       switch (optchar) {
           case 'h':
               help(argv[0]);
               return 1;
           case 'r':
               if (isatty(STDOUT_FILENO))
                   error(1, 0, "refuse to dump raw data to tty");
               raw = true;
               break;
           case 't':
               decoder_cfg.has_timestamp = true;
               break;
           case 's':
               decoder_cfg.src_bits = atoi(optarg);
               break;
           case 'y':
               synced = false;
               break;
           case 'b':
               bufsz = strtoul(optarg, NULL, 0);
               if (bufsz < NEXUS_RV_MSG_MAX_BYTES)
                   error(1, 0, "Buffer size cannot be smaller than %d",
                         NEXUS_RV_MSG_MAX_BYTES);
               break;
           default:
               return 1;
       }
    }
    if (argc == optind)
        error(1, 0, "Insufficient arguments");
    char *filename = argv[optind];
    char *offset_str = strchr(filename, ':');
    if (offset_str != NULL) {
        file_offset = strtoul(offset_str, NULL, 0);
        *offset_str = '\0';
    }
    int fd;
    if (!strcmp(filename, "-"))
        fd = STDIN_FILENO;
    else
        fd = open(filename, O_RDONLY);
    if (fd < 0)
        error(1, errno, "Failed to open file %s", filename);
    seek_file(fd, file_offset);
    dump(&decoder_cfg, stdout, fd, bufsz, synced, raw);
}