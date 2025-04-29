// SPDX-License-Identifer: Apache 2.0
/*
 * replay.c - Trace replayer utility that drives trace decoder
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <cstdlib>
#include <cinttypes>
#include <cassert>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <getopt.h>
#include <error.h>
#include <fcntl.h>
extern "C" {
#include <libnexus-rv/error.h>
#include <libnexus-rv/msg-decoder.h>
#include <libnexus-rv/trace-decoder.h>
#include <capstone.h>
}
#include "objfile.h"
#include "vm.h"
#include "linux.h"
#include "inst.h"
#include "sym.h"
#include "opts-def.h"
#include "misc.h"

#define DEFAULT_BUFFER_SIZE 4096

using namespace std;

static struct option long_opts[] = {
        {"help",      no_argument,       NULL, 'h'},
        {"tsbits",    required_argument, NULL, 't'},
        {"srcbits",   required_argument, NULL, 's'},
        {"buffersz",  required_argument, NULL, 'b'},
        {"sysroot",   required_argument, NULL, 'r'},
        {"debugdir",  required_argument, NULL, 'd'},
        {"ucore",     required_argument, NULL, 'u'},
        {NULL, 0,                        NULL, 0},
};

static const char short_opts[] = "hb:t:s:r:d:u:";

static void help(const char *argv0) {
    error(-1, 0, "Usage: \n"
                  "\t%s: [OPTIONS...] <trace file> or - for stdin\n"
                  "\n"
                  "\t-h, --help       Display this help message\n"
                  "\t-t, --tsbits     Bits of Timestamp, default 0\n"
                  "\t-s, --srcbits    Bits of SRC field, default 0\n"
                  "\t-b, --buffersz   Buffer size (default %d)\n"
                  "\t-r, --sysroot    Sysroot search dirs (affects following --ucore --kcore)\n"
                  "\t-d, --debugdir   Debug search dirs (affects following --ucore --kcore)\n"
                  "\t-u, --ucore      Userspace coredump (can be multiple)\n",
          argv0, DEFAULT_BUFFER_SIZE);
}

#define FMT_TIME_OFFSET "\n[%" PRIu64 "] +%zu "

map<shared_ptr<obj_file>, sym_server> sym_srvs;
unordered_map<uint64_t, shared_ptr<rv_inst_block> > insts;

static void print_sym(shared_ptr<memory_view> vm, FILE *fp, uint64_t addr,
                      const string **last_func) {
    auto [obj, vma] = vm->query_sym(addr);
    if (!obj)
        return;
    auto it = sym_srvs.find(obj);
    if (it == sym_srvs.end())
        it = sym_srvs.emplace(obj, obj->filename()).first;
    auto answer = it->second.query(vma);
    auto *filename = answer.filename;
    if (*last_func != answer.func)
        fprintf(fp, " %s", answer.func->c_str());
    else if (filename)
        fprintf(fp, " %*s", int(answer.func->size()), "");
    *last_func = answer.func;
    if (!filename)
        return;
    string_view short_fn;
    short_fn = *filename;
    auto it_fn = find(filename->rbegin(), filename->rend(), '/');
    if (it_fn != filename->rend())
        short_fn = string_view(it_fn.base(), filename->end());
    fprintf(fp, "; %.*s:%lu%s",
            int(short_fn.size()), short_fn.data(), answer.lineno,
            answer.note ? answer.note->c_str() : "");
}

static int align_print(int *printed, FILE *fp, int rc) {
    if (rc < 0)
        return rc;
    if (rc < *printed)
        rc = fprintf(fp, "%*s", *printed - rc, "");
    else
        *printed = rc;
    if (rc < 0)
        return rc;
    return *printed;
}

static void replay(shared_ptr<memory_view> vm, nexusrv_msg_decoder *msg_decoder, FILE *fp) {
    nexusrv_trace_decoder trace_decoder = {};
    int32_t rc = nexusrv_trace_decoder_init(&trace_decoder, msg_decoder);
    if (rc < 0)
        error(-rc, 0, "decoder_init failed: %s",
              str_nexus_error(-rc));
    uint64_t tnt_time = 0;
    uint64_t last_time = 0;
    std::optional<uint64_t> lastip;
    const string *last_func = nullptr;
    int addr_printed = 0, inst_printed = 0;
    for (;;) {
        nexusrv_msg msg;
        nexusrv_trace_indirect indir;
        nexusrv_trace_sync sync;
        nexusrv_trace_stop stop;
        nexusrv_trace_error err;
        shared_ptr<rv_inst_block> instblock;
        unsigned event = NEXUSRV_Trace_Event_Sync;
        rc = nexusrv_trace_sync_reset(&trace_decoder, &sync);
        if (rc < 0)
            error(-rc, 0, "sync_reset failed: %s",
                  str_nexus_error(-rc));
        if (rc > 0) {
            lastip.emplace(sync.addr);
            fprintf(fp,FMT_TIME_OFFSET " SYNC %u to 0x%" PRIx64,
                    nexusrv_trace_time(&trace_decoder),
                    nexusrv_msg_decoder_offset(msg_decoder),
                    sync.sync, *lastip);
            print_sym(vm, fp, *lastip, &last_func);
            goto check_time;
        }
        if (lastip.has_value()) {
            auto it = insts.find(*lastip);
            if (it == insts.end())
                it = insts.emplace(*lastip,rv_inst_block::fetch(
                        vm, *lastip, true)).first;
            instblock = it->second;
        }
        if (instblock) {
            align_print(&addr_printed, fp, fprintf(fp,
                        FMT_TIME_OFFSET " 0x%" PRIx64 ",+%" PRIu32 "  ",
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(msg_decoder),
                        *lastip, instblock->icnt));
            int inst_strlen = instblock->print_pre(fp);
            try {
                lastip.emplace(instblock->retire(&trace_decoder));
                int post_len = instblock->print_post(fp);
                if (inst_strlen > 0 && post_len >= 0)
                    align_print(&inst_printed, fp, inst_strlen + post_len);
                print_sym(vm, fp, instblock->addr, &last_func);
                continue;
            } catch (rv_inst_exc_event& exc_event) {
                lastip = exc_event.addr;
                event = exc_event.event;
                goto handle_event;
            } catch (rv_inst_exc_failed& failed) {
                rc = failed.rc;
            }
        } else
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
        if (lastip.has_value())
            lastip.emplace(*lastip + (uint32_t)rc * 2);
        if (rc) {
            tnt_time = 0;
            fprintf(fp,FMT_TIME_OFFSET "I-CNT %" PRIi32,
                    nexusrv_trace_time(&trace_decoder),
                    nexusrv_msg_decoder_offset(msg_decoder),
                    rc);
        }
handle_event:
        switch (event) {
            case NEXUSRV_Trace_Event_Direct: {
                /* Have no way to tell the branch target, reset lastip */
                lastip.reset();
                rc = nexusrv_trace_next_tnt(&trace_decoder);
                if (rc < 0)
                    error(-rc, 0, "next_tnt failed: %s",
                          str_nexus_error(-rc));
                if (tnt_time != nexusrv_trace_time(&trace_decoder))
                    fprintf(fp,FMT_TIME_OFFSET "TNT ",
                            nexusrv_trace_time(&trace_decoder),
                            nexusrv_msg_decoder_offset(msg_decoder));
                fprintf(fp, "%c", rc > 0 ? '!' : '.');
                tnt_time = nexusrv_trace_time(&trace_decoder);
                break;
            }
            case NEXUSRV_Trace_Event_Indirect:
            case NEXUSRV_Trace_Event_Trap: {
                rc = nexusrv_trace_next_indirect(&trace_decoder, &indir);
                if (rc < 0)
                    error(-rc, 0, "next_indir failed: %s",
                          str_nexus_error(-rc));
                lastip.emplace(indir.target);
                fprintf(fp,FMT_TIME_OFFSET "INDIRECT %s%s to 0x%" PRIx64,
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(msg_decoder),
                        indir.interrupt ? " interrupt" : "",
                        indir.exception ? " exception" : "",
                        *lastip);
                print_sym(vm, fp, indir.target, &last_func);
                if (indir.ownership)
                    fprintf(fp, " fmt=%u priv=%u v=%u context=0x%" PRIx64,
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
                lastip.emplace(sync.addr);
                fprintf(fp,FMT_TIME_OFFSET "SYNC %u to 0x%" PRIx64,
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(msg_decoder),
                        sync.sync, *lastip);
                print_sym(vm, fp, sync.addr, &last_func);
                break;
            }
            case NEXUSRV_Trace_Event_Stop: {
                lastip.reset();
                rc = nexusrv_trace_next_stop(&trace_decoder, &stop);
                if (rc < 0)
                    error(-rc, 0, "next_stop failed: %s",
                          str_nexus_error(-rc));
                fprintf(fp, FMT_TIME_OFFSET "STOP evcode=%u",
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(msg_decoder),
                        stop.evcode);
                break;
            }
            case NEXUSRV_Trace_Event_Error: {
                lastip.reset();
                rc = nexusrv_trace_next_error(&trace_decoder, &err);
                if (rc < 0)
                    error(-rc, 0, "next_error failed: %s",
                          str_nexus_error(-rc));
                fprintf(fp, FMT_TIME_OFFSET "ERROR etype=%u ecode=%u",
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(msg_decoder),
                        err.etype, err.ecode);
                break;
            }
        }
check_time:
        if (last_time && last_time > nexusrv_trace_time(&trace_decoder)) {
            error(0, 0, "WARN: Time goes backward, %" PRIu64 " vs %" PRIu64,
                  last_time, nexusrv_trace_time(&trace_decoder));
        }
        last_time = nexusrv_trace_time(&trace_decoder);
        continue;
unknown_msg:
        rc = nexusrv_msg_decoder_next(msg_decoder, &msg);
        if (rc < 0)
            error(-rc, 0, "msg_decoder_next failed: %s",
                  str_nexus_error(-rc));
        assert(rc > 0);
        fprintf(fp, "\n[%" PRIu64 "] UNKNOWN MSG ", nexusrv_trace_time(&trace_decoder));
        nexusrv_print_msg(fp, &msg);
    }
done_trace:
    nexusrv_trace_decoder_fini(&trace_decoder);
}

int main(int argc, char **argv) {
    nexusrv_hw_cfg hwcfg = {};
    hwcfg.ext_sifive = true; // Enable Sifive extension by default
    size_t bufsz = DEFAULT_BUFFER_SIZE;
    vector<string> sysroot_dirs = { "/" };
    vector<string> dbg_dirs = { "/usr/lib/debug" };
    vector<shared_ptr<linuxcore> > user_core_files;
    auto vm = make_shared<memory_view>();
    OPT_PARSE_BEGIN
    OPT_PARSE_H_HELP
    OPT_PARSE_T_TSBITS
    OPT_PARSE_S_SRCBITS
    OPT_PARSE_B_BUFSZ
    OPT_PARSE_U_UCORE
    OPT_PARSE_R_SYSROOT
    OPT_PARSE_D_DEBUGDIR
    OPT_PARSE_END
    if (argc == optind)
        error(-1, 0, "Insufficient arguments");
    char *filename = argv[optind];
    int fd = open_seek_file(filename, O_RDONLY | O_CLOEXEC);
    unique_ptr<uint8_t[]> buffer = make_unique<uint8_t[]>(bufsz);
    nexusrv_msg_decoder msg_decoder = {};
    nexusrv_msg_decoder_init(&msg_decoder, &hwcfg, fd, buffer.get(), bufsz);
    replay(vm, &msg_decoder, stdout);
    close(fd);
    return 0;
}