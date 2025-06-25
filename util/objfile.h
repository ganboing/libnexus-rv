// SPDX-License-Identifier: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_OBJFILE_H
#define LIBNEXUS_RV_OBJFILE_H

#include <bfd.h>
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <vector>
#include <map>
#include <sys/mman.h>
#include "misc.h"

struct obj_file : std::enable_shared_from_this<obj_file> {
    obj_file(const char *filename, bfd_format format);
    std::pair<void*, size_t> mmap(uint64_t fileoff,
                                    int prot = PROT_READ, int flags = MAP_SHARED);
    void unmap(uint64_t fileoff);
    uint64_t fileoff_to_va(uint64_t offset) const;
    std::optional<std::vector<uint8_t> > get_build_id() const;
    inline const char *filename() const {
        return abfd->filename;
    }
    inline bfd *get_bfd() const {
        return abfd.get();
    }
    ~obj_file();
protected:
    auto_fd autofd;
    std::unique_ptr<bfd, decltype(&bfd_close)> abfd;
    std::map<uint64_t, asection*> sect_by_vma;
    std::map<uint64_t, asection*> sect_by_fileoff;
    std::map<uint64_t, void*> sect_mapped;
    std::multimap<std::string_view, asection*> sect_by_name;
public:
    inline const std::map<uint64_t, asection*>& get_sect_by_vma() const {
        return sect_by_vma;
    }
    inline const std::map<uint64_t, asection*>& get_sect_by_fileoff() const {
        return sect_by_fileoff;
    }
    inline const std::multimap<std::string_view, asection*>& get_sec_by_name() const {
        return sect_by_name;
    };
};

struct objfile_store : std::enable_shared_from_this<objfile_store> {
    inline virtual std::shared_ptr<obj_file> get(const char *) const {
        return nullptr;
    }
    inline virtual std::shared_ptr<obj_file> get_dbg(const char *) const {
        return nullptr;
    }
    inline virtual std::shared_ptr<obj_file> get_dbg_buildid(
            const std::vector<uint8_t>&) const {
        return nullptr;
    }
};

struct single_objstore : objfile_store {
    inline single_objstore(std::shared_ptr<obj_file> obj) : obj(obj) {}
    inline std::shared_ptr<obj_file> get(const char *filename) const override {
        if (!strcmp(obj->filename(), filename))
            return obj;
        return nullptr;
    }
protected:
    std::shared_ptr<obj_file> obj;
};

struct core_file : obj_file {
    inline core_file(const char *filename, bfd_format format = bfd_core) :
        obj_file(filename, format) {}
    /* <Filename, File offset> */
    inline virtual std::tuple<const std::string*, uint64_t>
        get_file_backing(uint64_t) {
        return std::tuple(nullptr, 0);
    }
    /* <Filename, Section name, Section offset or VMA> */
    inline virtual std::tuple<const std::string*, const std::string*, uint64_t>
        get_file_vma(uint64_t) {
        return std::tuple(nullptr, nullptr, 0);
    }
    /* <Label, Module, Address> */
    inline virtual std::tuple<const std::string*, const std::string*, uint64_t>
        get_label(uint64_t) {
        return std::tuple(nullptr, nullptr, 0);
    }
    inline virtual std::shared_ptr<objfile_store> obj_store() {
        return nullptr;
    }
};

struct elf_file : core_file {
    inline elf_file(const char *filename) :
        core_file(filename, bfd_object) {
        obj_filename = this->filename();
    }
    inline std::tuple<const std::string*, const std::string*, uint64_t>
        get_file_vma(uint64_t vma) override {
        return std::make_tuple(&obj_filename, nullptr, vma);
    }
    inline std::shared_ptr<objfile_store> obj_store() override {
        return std::make_shared<single_objstore>(shared_from_this());
    }
protected:
    std::string obj_filename;
};

#endif