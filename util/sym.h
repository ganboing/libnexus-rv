#ifndef LIBNEXUS_RV_SYM_H
#define LIBNEXUS_RV_SYM_H

#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "misc.h"

struct sym_answer {
    const std::string *func;
    const std::string *filename;
    const unsigned long lineno;
    const std::string *note;
};

struct sym_server {
    sym_server(const char *filename, const char *section = nullptr);
    sym_answer query(uint64_t addr);
    inline ~sym_server() {
        free(line);
    }
private:
    std::unordered_set<std::string> strings;
    std::unordered_map<uint64_t, sym_answer> cached;
    char *line = nullptr;
    size_t linesz = 0;
    auto_file read_fp;
    auto_file write_fp;
};

void sym_iterate_kallsyms(FILE *fp, const std::function<
        bool(uint64_t, char, const char *, const char*)>&    f);

#endif
