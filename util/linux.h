// SPDX-License-Identifier: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_LINUX_H
#define LIBNEXUS_RV_LINUX_H

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include "objfile.h"

auto constexpr no_map = std::make_tuple(nullptr, 0);
auto constexpr no_map_or_sym = std::make_tuple(nullptr, nullptr, 0);

struct linux_file_store : objfile_store {
    inline linux_file_store(std::vector<std::string> prefixes,
                            std::vector<std::string> dbg_prefixes) :
            prefixes(std::move(prefixes)),
            dbg_prefixes(std::move(dbg_prefixes)) {}
    std::shared_ptr<obj_file> get(const char *filename) const override;
    std::shared_ptr<obj_file> get_dbg(const char *filename) const override;
    std::shared_ptr<obj_file> get_dbg_buildid(
            const std::vector<uint8_t>& buildid) const override;
protected:
    static std::shared_ptr<obj_file> search_in(const std::vector<std::string>& dirs,
                                               const char *filename,
                                               bfd_format format);
    std::vector<std::string> prefixes;
    std::vector<std::string> dbg_prefixes;
};

struct linux_kmods_file_store : linux_file_store {
    inline linux_kmods_file_store(std::vector<std::string> prefixes,
                                  std::vector<std::string> dbg_prefixes,
                                  std::string kernel_version);
    std::shared_ptr<obj_file> get(const char *mod) const override;
    std::shared_ptr<obj_file> get_dbg(const char *mod) const override;
    static std::map<std::string, std::string> parse_moddep(
            const std::filesystem::path& moddep);
private:
    std::string kernel_version;
    std::map<std::string, std::string> moddep;
};

struct linux_file_backing {
    uint64_t start;
    uint64_t size;
    uint64_t file_offset;
    const std::string *filename;
};


struct linux_core_file : core_file {
    inline linux_core_file(const char *filename) :
            core_file(filename) {}
    inline linux_core_file(const char *filename,
                           std::vector<std::string> sysroot_dirs,
                           std::vector<std::string> dbg_dirs) :
            core_file(filename) {
        store = std::make_shared<linux_file_store>(
                        std::move(sysroot_dirs), std::move(dbg_dirs));
    }
    inline std::shared_ptr<objfile_store> obj_store() override {
        return store;
    }
protected:
    std::shared_ptr<linux_file_store> store;
};

struct linux_ucore : linux_core_file {
    linux_ucore(const char *filename,
                std::vector<std::string> sysroot_dirs = {},
                std::vector<std::string> dbg_dirs = {});
    std::tuple<const std::string*, uint64_t>
        get_file_backing(uint64_t vma) override;
private:
    std::set<std::string> file_names;
    std::map<uint64_t, linux_file_backing> mmap_backing;
    template<typename INTTYPE>
    friend void parse_ucore_metadata(linux_ucore *core,
                                    const std::vector<uint8_t>& sec_data);
};

struct linux_kcore : linux_core_file {
    linux_kcore(const char *procfs, const char *sysfs,
                std::vector<std::string> sysroot_dirs = {},
                std::vector<std::string> dbg_dirs = {});
    std::tuple<const std::string*, const std::string*, uint64_t>
        get_file_vma(uint64_t vma) override;
    std::tuple<const std::string*, const std::string*, uint64_t>
        get_label(uint64_t) override;
private:
    uint64_t vmlinux_start;
    uint64_t vmlinux_end;
    uint64_t bpf_start;
    uint64_t kaslr_offset;
    std::string kernel_release;
    std::set<std::string> kallsyms_mods;
    std::multimap<uint64_t, std::tuple<std::string, char, const std::string*> > kallsyms;
    std::map<std::string, std::map<std::string, uint64_t> > mod_sections;
    std::map<std::string_view, std::vector<uint8_t> > mod_buildids;
    std::map<uint64_t, std::pair<const std::string*, const std::string*> > sections_map;
};

std::vector<std::string> split_dirs(const std::string& str);

#endif
