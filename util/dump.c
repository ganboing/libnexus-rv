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
#include "print.h"
#include "misc.h"

#define DEFAULT_BUFFER_SIZE 4096

static struct option LongOpts[] = {
        {"help", no_argument, NULL, 'h'},
        {"timestamp", no_argument, NULL, 't'},
        {"srcbits", required_argument, NULL, 's'},
        {"sync", no_argument, NULL, 'y'},
        {"buffersz", required_argument, NULL, 'b'},
        { NULL, 0, NULL, 0 },
};

static const char ShortOpts[] = "hb:yts:";

static void help(const char *Argv0) {
    error(1, 0, "Usage: \n"
                    "\t%s: [OPTIONS...] <trace file> or - for stdin\n"
                    "\n"
                    "\t-h, --help       Display this help message\n"
                    "\t-t, --timestamp  Enable timestamp\n"
                    "\t-s, --srcbits    Bits of SRC field, default 0\n"
                    "\t-b, --buffersz   Buffer size (default %d)\n"
                    "\n", Argv0, DEFAULT_BUFFER_SIZE);
}

static ssize_t bufferFill(int Fd, uint8_t *Buffer, size_t CarryOver, size_t Total) {
    memmove(Buffer, Buffer + Total - CarryOver, CarryOver);
    ssize_t Ret = readAll(Fd, Buffer + CarryOver, Total - CarryOver);
    if (Ret < 0)
        return Ret;
    return CarryOver + Ret;
}

static int dump(NexusRvDecoderCfg *Cfg, int Fd, size_t Offset, size_t BufferSz) {
    ssize_t MsgId = 0;
    bool Seekable = true;
    ssize_t Seeked = lseek(Fd, Offset, SEEK_SET);
    if (Seeked < 0) {
        if (errno == ESPIPE)
            Seekable = false;
        else
            error(1, errno, "Failed to seek file");
    }
    // Take care of unseekable pipe file
    if (!Seekable) {
        Seeked = seekPipe(Fd, Offset);
        if (Seeked < 0)
            error(1, errno, "Failed to seek pipe");
        if (Seeked != Offset)
            error(1, 0, "Short read of pipe");
    }
    uint8_t *Buffer = malloc(BufferSz);
    if (!Buffer)
        error(1, 0, "Failed to allocate buffer");
    // Begin with empty buffer
    size_t Pos = BufferSz;
    bool Eof = false;
    ssize_t Ret;
    for (;;) {
        NexusRvMsg Msg;
        // First attempt will fail with -nexus_stream_truncate (expected)
        Ret = nexusrv_decode(Cfg, Buffer + Pos, BufferSz - Pos, &Msg);
        if (Ret >= 0) {
            Pos += Ret;
            printf("Msg #%zu ", MsgId++);
            printMsg(stdout, &Msg);
            continue;
        }
        if (Ret != -nexus_stream_truncate)
            break;
        if (Eof) {
            Ret = 0; // Leftover bytes OK
            break;
        }
        if (BufferSz - Pos >= NEXUS_RV_MSG_MAX_BYTES)
            error(1, 0,
                  "Should not have a left over partial msg of >= %d bytes", NEXUS_RV_MSG_MAX_BYTES);
        ssize_t Read = bufferFill(Fd, Buffer, BufferSz - Pos, BufferSz);
        if (Read < 0) {
            Ret = Read;
            break;
        }
        Pos = 0;
        Eof = Read < BufferSz;
        BufferSz = Read;
    }
    free(Buffer);
    return Ret;
}

int main(int argc, char **argv) {
    int OptIdx = 0;
    int OptChar;
    NexusRvDecoderCfg DecodeCfg = {};
    bool Sync = false;
    size_t FileOffset = 0;
    size_t BufferSz = DEFAULT_BUFFER_SIZE;
    while ((OptChar = getopt_long(
            argc, argv,
            ShortOpts, LongOpts, &OptIdx)) != -1) {
       switch (OptChar) {
           case 'h':
               help(argv[0]);
               return 1;
           case 't':
               DecodeCfg.HasTimeStamp = true;
               break;
           case 's':
               DecodeCfg.SrcBits = atoi(optarg);
               break;
           case 'y':
               Sync = true;
               break;
           case 'b':
               BufferSz = strtoul(optarg, NULL, 0);
               if (BufferSz < NEXUS_RV_MSG_MAX_BYTES)
                   error(1, 0, "Buffer size cannot be smaller than %d", NEXUS_RV_MSG_MAX_BYTES);
               break;
           default:
               return 1;
       }
    }
    if (argc - optind < 1)
        error(1, 0, "Insufficient arguments");
    char *FileName = argv[optind];
    char *Offset = strchr(FileName, ':');
    if (Offset != NULL) {
        FileOffset = strtoul(Offset, NULL, 0);
        *Offset = '\0';
    }
    int Fd;
    if (!strcmp(FileName, "-"))
        Fd = STDIN_FILENO;
    else
        Fd = open(FileName, O_RDONLY);
    if (Fd < 0)
        error(1, errno, "Failed to open file %s", FileName);
    return dump(&DecodeCfg, Fd, FileOffset, BufferSz);
}