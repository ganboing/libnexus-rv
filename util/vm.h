#ifndef LIBNEXUS_RV_VM_H
#define LIBNEXUS_RV_VM_H

#include <string_view>
#include "objfile.h"
#include "linux.h"

struct memory_view {
    void load_core(std::shared_ptr<linuxcore> coredump);
    std::pair<const uint8_t*, size_t> try_map(uint64_t vma);
    std::pair<std::shared_ptr<obj_file>, uint64_t> query_sym(uint64_t vma);
private:
    struct loaded_section {
        std::shared_ptr<linuxcore> core;
        bfd_section *asection;
    };
    std::map<uint64_t, loaded_section> loaded_sections;
    std::map<const char*, std::pair<
        std::shared_ptr<obj_file>,
        std::shared_ptr<obj_file>> > backed_file_cache;
};

#endif
