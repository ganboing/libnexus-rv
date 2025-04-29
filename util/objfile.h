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

struct obj_file_store {
    inline obj_file_store(std::vector<std::string> prefixes,
                          std::vector<std::string> dbg_prefixes) :
                          prefixes(std::move(prefixes)),
                          dbg_prefixes(std::move(dbg_prefixes)) {}
    std::shared_ptr<obj_file> get(const char *filename,
                                  bfd_format format = bfd_object) const;
    std::shared_ptr<obj_file> get_dbg(const char *filename,
                                      bfd_format format = bfd_object) const;
    std::shared_ptr<obj_file> get_dbg_buildid(const std::vector<uint8_t>& buildid,
                                              bfd_format format = bfd_object) const;
private:
    static std::shared_ptr<obj_file> search_in(const std::vector<std::string>& dirs,
                                               const char *filename,
                                               bfd_format format);
    std::vector<std::string> prefixes;
    std::vector<std::string> dbg_prefixes;
};

std::vector<std::string> split_dirs(const std::string& str);

#endif