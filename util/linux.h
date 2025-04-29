#ifndef LIBNEXUS_RV_LINUX_H
#define LIBNEXUS_RV_LINUX_H

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include "objfile.h"

struct linux_file_backing {
    uint64_t start;
    uint64_t size;
    uint64_t file_offset;
    const char *filename;
};

struct linuxcore : obj_file {
    linuxcore(const char *filename,
              std::vector<std::string> sysroot_dirs = {},
              std::vector<std::string> dbg_dirs = {});
    std::pair<const char*, uint64_t> get_file_backing(uint64_t vma);
    inline const obj_file_store& get_obj_store() const {
        return obj_store;
    }
private:
    std::set<std::string> file_names;
    std::map<uint64_t, linux_file_backing> mmap_backing;
    obj_file_store obj_store;
    template<typename INTTYPE>
    friend void parse_core_metadata(linuxcore *core,
                                    const std::vector<uint8_t>& sec_data);
};

#endif
