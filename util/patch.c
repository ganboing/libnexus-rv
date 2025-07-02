// SPDX-License-Identifier: Apache 2.0
/*
 * patch.c - Patch message stream
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <getopt.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <libnexus-rv/error.h>
#include <libnexus-rv/msg-decoder.h>
#include <libnexus-rv/msg-encoder.h>
#include "opts-def.h"
#include "misc.h"

#define DEFAULT_BUFFER_SIZE 4096

static struct option long_opts[] = {
    {"help",      no_argument,       NULL, 'h'},
    {"hwcfg",     required_argument, NULL, 'w'},
    {"buffersz",  required_argument, NULL, 'b'},
    {NULL, 0,                        NULL, 0},
};

static const char short_opts[] = "hw:s:c:";

static void help(const char *argv0) {
    error(-1, 0, "Usage: \n"
                    "\t%s: [OPTIONS...] <trace file or - for stdin> <cmd...> \n"
                    "\n"
                    "\t-h, --help            Display this help message\n"
                    "\t-w, --hwcfg [string]  Hardware Configuration string\n"
                    "\t-b, --buffersz [int]  Buffer size (default %d)\n",
                    argv0, DEFAULT_BUFFER_SIZE);
}

static void patch(nexusrv_hw_cfg *hwcfg, int fd, size_t bufsz, char *cmd) {
    static const char cmd_seek[] = "seek=";
    static const char cmd_show[] = "show";
    static const char cmd_next[] = "next";
    static const char cmd_icnt[] = "icnt=";
    void *buffer = malloc(bufsz);
    if (!buffer)
        error(-1, 0, "Failed to allocate buffer");
    char *saveptr;
    size_t fileoff = 0;
    nexusrv_msg_decoder msg_decoder = {};
    nexusrv_msg_decoder_init(&msg_decoder, hwcfg, fd, -1, buffer, bufsz);
    for (char *c = strtok_r(cmd, ",", &saveptr); c;
            c = strtok_r(NULL, ",", &saveptr)) {
        if (!strncmp(c, cmd_seek, sizeof(cmd_seek) - 1)) {
            fileoff = strtoul(c + sizeof(cmd_seek) - 1, NULL, 0);
        } else {
            nexusrv_msg msg;
            ssize_t rc = nexusrv_msg_decoder_next(&msg_decoder, &msg);
            if (rc < 0)
                error(-rc, 0, "Failed to decode msg: %s",
                    str_nexus_error(-rc));
            if (!rc) {
                error(0, 0, "EOF on read");
                exit(0);
            }
            if (!strcmp(c, cmd_next)) {
                continue;
            }
            size_t cur_off = nexusrv_msg_decoder_offset(&msg_decoder);
            if (!strcmp(c, cmd_show)) {
                fprintf(stdout, "Msg +%zu ", cur_off + fileoff);
                nexusrv_print_msg(stdout, &msg);
                fputc('\n', stdout);
            } else if (!strncmp(c, cmd_icnt, sizeof(cmd_icnt) - 1)) {
                if (!nexusrv_msg_has_icnt(&msg))
                    error(-1, 0, "msg has no i-cnt field");
                msg.icnt = strtoul(c + sizeof(cmd_icnt) - 1, NULL, 0);
                uint8_t bytes[rc];
                memset(bytes, 0xff, rc);
                ssize_t rc2 = nexusrv_msg_encode(hwcfg, bytes, rc, &msg);
                if (rc2 < 0)
                    error(-rc2, 0, "Failed to encode msg: %s",
                        str_nexus_error(-rc2));
                rc2 = pwrite(fd, bytes, rc, cur_off + fileoff);
                if (rc2 < 0)
                    error(-1, errno, "pwrite failed");
                if (rc2 != rc)
                    error(-1, 0, "short pwrite");
            } else
                error(-1, 0, "unknown command %s", c);
            fileoff += cur_off;
        }
        // Re-init
        off_t seeked = lseek(fd, fileoff, SEEK_SET);
        if (seeked < 0 || seeked != fileoff)
            error(-1, errno, "Failed to seek file");
        nexusrv_msg_decoder_init(&msg_decoder, hwcfg, fd, -1, buffer, bufsz);
    }
    free(buffer);
}

int main(int argc, char **argv) {
    nexusrv_hw_cfg hwcfg = {};
    const char *hwcfg_str = "generic64";
    size_t bufsz = DEFAULT_BUFFER_SIZE;
    OPT_PARSE_BEGIN
    OPT_PARSE_H_HELP
    OPT_PARSE_W_HWCFG
    OPT_PARSE_B_BUFSZ
    OPT_PARSE_END
    if (argc < optind + 2)
        error(-1, 0, "Insufficient arguments");
    if (nexusrv_hwcfg_parse(&hwcfg, hwcfg_str))
        error(-1, 0, "Invalid hwcfg string");
    char *filename = argv[optind];
    char *cmd = argv[optind + 1];
    int fd = open_seek_file(filename, O_RDWR);
    patch(&hwcfg, fd, bufsz, cmd);
    close(fd);
    return 0;
}