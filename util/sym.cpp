// SPDX-License-Identifier: Apache 2.0
/*
 * sym.cpp - Symbol resolver
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <error.h>
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <fcntl.h>
#include "sym.h"

using namespace std;

#define xstr(s) str(s)
#define str(s) #s

const char *exe_addr2line = []{
   const char *env = getenv("ADDR2LINE");
   if (env)
       return env;
   return DEFAULT_ADDR2LINE;
}();

sym_server::sym_server(const char *filename, const char *section) :
read_fp(nullptr, fclose), write_fp(nullptr, fclose) {
    union {
        struct {
            int client_r;
            int server_w;
            int server_r;
            int client_w;
        };
        int fd_pair[2][2];
    } fds;
    for (unsigned i = 0; i < 2; ++i)
        if (pipe2(fds.fd_pair[i], O_CLOEXEC) < 0)
            error(-1, errno, "pipe failed:");
    pid_t pid = fork();
    if (pid < 0)
        error(-1, errno, "fork failed");
    if (pid) {
        // The parent is the client
        close(fds.server_r);
        close(fds.server_w);
        read_fp.reset(fdopen(fds.client_r, "r"));
        write_fp.reset(fdopen(fds.client_w, "w"));
        setlinebuf(write_fp.get());
    } else {
        // The child is the server
        close(fds.client_r);
        close(fds.client_w);
        if (dup2(fds.server_r, STDIN_FILENO) < 0 ||
            dup2(fds.server_w, STDOUT_FILENO) < 0)
            error(-1, errno, "dup2 failed");
        close(fds.server_r);
        close(fds.server_w);
        const char *args[] = {exe_addr2line, "-f", "-e", filename,
                              nullptr, nullptr, nullptr};
        if (section) {
            args[4] = "-j";
            args[5] = section;
        }
        execvp(exe_addr2line, (char**)args);
        error(-1, errno, "exec addr2line failed");
    }
}

sym_answer sym_server::query(uint64_t addr) {
    auto it = cached.find(addr);
    if (it != cached.end())
        return it->second;
    fprintf(write_fp.get(), "0x%" PRIx64 "\n", addr);
    ssize_t rc = getline(&line, &linesz, read_fp.get());
    if (rc <= 0)
        error(-1, errno, "getline failed (function)");
    const string *func = &*strings.emplace(line, rc - 1).first;
    rc = getline(&line, &linesz, read_fp.get());
    if (rc <= 0)
        error(-1, errno, "getline failed (filename/notes)");
    auto cpos = strrchr(line, ':');
    const string *filename = nullptr;
    unsigned long lineno = 0;
    const string *note = nullptr;
    if (cpos) {
        filename = &*strings.emplace(line, cpos).first;
        lineno = strtoul(cpos + 1, &cpos, 10);
        if (cpos != line + rc - 1)
            note = &*strings.emplace(cpos, line + rc - 1).first;
    }
    it = cached.emplace(addr, sym_answer{func, filename, lineno, note}).first;
    return it->second;
}

void sym_iterate_kallsyms(FILE *fp, const function<
        bool(uint64_t, char, const char *, const char*)>& f) {
    char *line = nullptr;
    size_t sz = 0;
    ssize_t rc;
    while ((rc = getline(&line, &sz, fp)) > 0) {
        if (line[rc - 1] == '\n')
            line[rc - 1] = '\0';
        char *type = strchr(line, ' ');
        if (!type)
            error(-1, 0, "cannot find symbol type, kallsyms str=%s", line);
        *type++ = '\0';
        char *name = strchr(type, ' ');
        if (!name)
            error(-1, 0, "cannot find symbol name, kallsyms str=%s", type);
        ++name;
        char *module = strchr(name, '\t');
        if (module)
            *module++ = '\0';
        if (!f(strtoull(line, nullptr, 16), *type, name, module))
            break;
    }
    free(line);
}