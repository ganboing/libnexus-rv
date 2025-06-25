// SPDX-License-Identifier: Apache 2.0
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
        {"hwcfg",     required_argument, NULL, 'w'},
        {"filter",    required_argument, NULL, 'c'},
        {"buffersz",  required_argument, NULL, 'b'},
        {"elf",       required_argument, NULL, 'e'},
        {"sysroot",   required_argument, NULL, 'r'},
        {"debugdir",  required_argument, NULL, 'd'},
        {"procfs",    required_argument, NULL, 'p'},
        {"sysfs",     required_argument, NULL, 'y'},
        {"ucore",     required_argument, NULL, 'u'},
        {"kcore",     no_argument,       NULL, 'k'},
        {NULL, 0,                        NULL, 0},
};

static const char short_opts[] = "hw:s:c:b:e:r:d:p:y:u:k";

static void help(const char *argv0) {
    error(-1, 0, "Usage: \n"
                  "\t%s: [OPTIONS...] <trace file> or - for stdin\n"
                  "\n"
                  "\t-h, --help            Display this help message\n"
                  "\t-w, --hwcfg [string]  Hardware Configuration string\n"
                  "\t-c, --filter [int]    Select a particular SRC (hart)\n"
                  "\t-b, --buffersz [int]  Buffer size (default %d)\n"
                  "\t-e, --elf [path]      Path to ELF file to load as core/symbol\n"
                  "\t-p, --procfs [path]   Path to procfs (default /proc)\n"
                  "\t-y, --sysfs [path]    Path to sysfs (default /sys)\n"
                  "\t-r, --sysroot [path:path:...]\n"
                  "\t                      Sysroot search dirs (affects following --ucore --kcore)\n"
                  "\t-d, --debugdir [path:path:...]\n"
                  "\t                      Debug search dirs (affects following --ucore --kcore)\n"
                  "\t-u, --ucore [path]    Userspace coredump (can be multiple)\n"
                  "\t-k, --kcore           Kernel coredump (using {procfs}/kcore)\n",
          argv0, DEFAULT_BUFFER_SIZE);
}

#define FMT_TIME_OFFSET "\n[%" PRIu64 "] +%zu "

map<shared_ptr<obj_file>, map<string, sym_server> > sym_srvs;
unordered_map<uint64_t, shared_ptr<rv_inst_block> > insts;

static void print_label(shared_ptr<memory_view> vm, FILE *fp, uint64_t addr,
                        const string **last_func) {
    auto [func, module, vma] = vm->query_label(addr);
    if (!func)
        return;
    if (*last_func != func)
        fprintf(fp, " %s", func->c_str());
    else if (module)
        fprintf(fp, " %*s", int(func->size()), "");
    *last_func = func;
    if (module)
        fprintf(fp, "; %s", module->c_str());
}

static void print_sym(shared_ptr<memory_view> vm, FILE *fp, uint64_t addr,
                      const string **last_func) {
    auto [obj, section, vma] = vm->query_sym(addr);
    if (!obj) {
        print_label(vm, fp, addr, last_func);
        return;
    }
    auto& srvs = sym_srvs[obj];
    auto it = srvs.try_emplace(
            section ? *section : "",
            obj->filename(),
            section ? section->c_str() : nullptr).first;
    auto answer = it->second.query(vma);
    auto *filename = answer.filename;
    if (*last_func != answer.func)
        fprintf(fp, " %s", answer.func->c_str());
    else if (filename)
        fprintf(fp, " %*s", int(answer.func->size()), "");
    *last_func = answer.func;
    if (!filename)
        return;
    string_view short_fn(*filename);
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
    optional<uint64_t> lastip;
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
            event = NEXUSRV_Trace_Event_None;
            unsigned stack = nexusrv_trace_callstack_used(&trace_decoder);
            try {
                lastip.emplace(instblock->retire(&trace_decoder));
            } catch (rv_inst_exc_event& exc_event) {
                event = exc_event.event;
                lastip.emplace(exc_event.addr);
                assert(event != NEXUSRV_Trace_Event_None);
                assert(*lastip >= instblock->addr &&
                       *lastip - instblock->addr < instblock->icnt * 2);
            } catch (rv_inst_exc_failed& failed) {
                rc = failed.rc;
                assert(rc < 0);
                goto handle_error;
            }
            align_print(&addr_printed, fp, fprintf(fp,
                        FMT_TIME_OFFSET " 0x%" PRIx64 ",+%" PRIu32 "  ",
                        nexusrv_trace_time(&trace_decoder),
                        nexusrv_msg_decoder_offset(msg_decoder),
                        instblock->addr, instblock->icnt));
            if (event == NEXUSRV_Trace_Event_None) {
                align_print(&inst_printed, fp, instblock->print(fp));
                // Indent with stack depth
                fprintf(fp, " │ %*s", stack, "");
                print_sym(vm, fp, instblock->addr, &last_func);
                continue;
            }
            auto insn = rv_inst_block::disasm1(vm, *lastip, true);
            align_print(&inst_printed, fp, fprintf(
                    fp, "[retired %" PRIu64 "] %s%s%s",
                    (*lastip - instblock->addr) / 2,
                    insn ? insn->mnemonic : "",
                    insn ? " " : "",
                    insn ? insn->op_str : ""));
            goto handle_event;
        } else
            rc = nexusrv_trace_try_retire(&trace_decoder, UINT32_MAX, &event);
handle_error:
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
            case NEXUSRV_Trace_Event_Direct:
            case NEXUSRV_Trace_Event_DirectSync: {
                /* Have no way to tell the branch target, reset lastip */
                lastip.reset();
                if (tnt_time != nexusrv_trace_time(&trace_decoder))
                    fprintf(fp,FMT_TIME_OFFSET "TNT ",
                            nexusrv_trace_time(&trace_decoder),
                            nexusrv_msg_decoder_offset(msg_decoder));
                tnt_time = nexusrv_trace_time(&trace_decoder);
                rc = nexusrv_trace_next_tnt(&trace_decoder);
                if (rc < 0)
                    error(-rc, 0, "next_tnt failed: %s",
                          str_nexus_error(-rc));
                fprintf(fp, "%c", rc > 0 ? '!' : '.');
                break;
            }
            case NEXUSRV_Trace_Event_Indirect:
            case NEXUSRV_Trace_Event_IndirectSync:
            case NEXUSRV_Trace_Event_Trap: {
                rc = nexusrv_trace_next_indirect(&trace_decoder, &indir);
                if (rc < 0)
                    error(-rc, 0, "next_indir failed: %s",
                          str_nexus_error(-rc));
                lastip.emplace(indir.target);
                fprintf(fp,FMT_TIME_OFFSET "INDIRECT%s%s to 0x%" PRIx64,
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
        nexusrv_trace_add_timestamp(&trace_decoder, msg.timestamp);
        fprintf(fp, "\n[%" PRIu64 "] UNKNOWN MSG ", nexusrv_trace_time(&trace_decoder));
        nexusrv_print_msg(fp, &msg);
    }
done_trace:
    nexusrv_trace_decoder_fini(&trace_decoder);
}

int main(int argc, char **argv) {
    nexusrv_hw_cfg hwcfg = {};
    const char *hwcfg_str = "generic64";
    int16_t cpu = -1;
    size_t bufsz = DEFAULT_BUFFER_SIZE;
    const char *sysfs = "/sys";
    const char *procfs = "/proc";
    vector<string> sysroot_dirs = { "/" };
    vector<string> dbg_dirs = { "/usr/lib/debug" };
    auto vm = make_shared<memory_view>();
    OPT_PARSE_BEGIN
    OPT_PARSE_H_HELP
    OPT_PARSE_W_HWCFG
    OPT_PARSE_C_CPU
    OPT_PARSE_B_BUFSZ
    OPT_PARSE_U_UCORE
    OPT_PARSE_R_SYSROOT
    OPT_PARSE_D_DEBUGDIR
    OPT_PARSE_P_PROCFS
    OPT_PARSE_Y_SYSFS
    OPT_PARSE_K_KCORE
    OPT_PARSE_E_ELF
    OPT_PARSE_END
    if (argc == optind)
        error(-1, 0, "Insufficient arguments");
    if (nexusrv_hwcfg_parse(&hwcfg, hwcfg_str))
        error(-1, 0, "Invalid hwcfg string");
    char *filename = argv[optind];
    int fd = open_seek_file(filename, O_RDONLY | O_CLOEXEC);
    unique_ptr<uint8_t[]> buffer = make_unique<uint8_t[]>(bufsz);
    nexusrv_msg_decoder msg_decoder = {};
    nexusrv_msg_decoder_init(&msg_decoder,
                             &hwcfg, fd, cpu, buffer.get(), bufsz);
    replay(vm, &msg_decoder, stdout);
    close(fd);
    return 0;
}