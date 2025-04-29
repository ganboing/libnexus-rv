#include <bfd.h>
#include <error.h>
#include <algorithm>
#include "linux.h"

using namespace std;

template<typename INTTYPE>
struct riscv_cf_mapping {
    INTTYPE count;
    INTTYPE page_size;
    struct {
        INTTYPE start;
        INTTYPE end;
        INTTYPE file_ofs;
    } files[];
};

template<typename INTTYPE>
void parse_core_metadata(linuxcore *core, const vector<uint8_t>& sec_data) {
    const riscv_cf_mapping<INTTYPE> *mapping =
            reinterpret_cast<decltype(mapping)>(sec_data.data());
    if (sec_data.size() < sizeof(*mapping))
        error(-1, 0, ".note.linuxcore.file too short");
    if (sec_data.size() < sizeof(*mapping) +
        sizeof(mapping->files[0]) * mapping->count)
        error(-1, 0, ".note.linuxcore.file too short");
    const char *filenames = reinterpret_cast<const char*>(&mapping->files[mapping->count]);
    const char *end = reinterpret_cast<const char*>(sec_data.data() + sec_data.size());
    for (INTTYPE i = 0; i < mapping->count; ++i) {
        auto [it, inserted] = core->mmap_backing.emplace(mapping->files[i].start,
            linux_file_backing{
                mapping->files[i].start,
                mapping->files[i].end - mapping->files[i].start,
                mapping->files[i].file_ofs * mapping->page_size});
        if (!inserted)
            error(-1, 0, "duplicate file backed mmap at %" PRIx64
                " filename %s", it->first, it->second.filename);
        auto nullbyte = find(filenames, end, '\0');
        if (nullbyte == end)
            error(1, 0, "Can not find next file name");
        it->second.filename = core->file_names.emplace(filenames).first->c_str();
        filenames = nullbyte + 1;
    }
}

linuxcore::linuxcore(const char *filename,
                     vector<string> sysroot_dirs, vector<string> dbg_dirs) :
        obj_file(filename, bfd_core),
        obj_store(move(sysroot_dirs), move(dbg_dirs))
{
    asection *section = bfd_get_section_by_name(abfd.get(),
                                                ".note.linuxcore.file");
    if (!section)
        error(-1, 0, "section .note.linuxcore.file not found");
    vector<uint8_t> sec_data;
    sec_data.resize(bfd_section_size(section));
    if (!bfd_get_section_contents(abfd.get(), section, sec_data.data(),
                                  0, sec_data.size()))
        error(-1, 0, "failed to get section data");
    unsigned long march = bfd_get_mach(abfd.get());
    switch (march) {
        case bfd_mach_riscv32:
            parse_core_metadata<uint32_t>(this, sec_data);
            break;
        case bfd_mach_riscv64:
            parse_core_metadata<uint64_t>(this, sec_data);
            break;
        default:
            error(-1, 0,"unknown march %lu", march);
    }
}

pair<const char*, uint64_t> linuxcore::get_file_backing(uint64_t vma) {
    auto it = mmap_backing.upper_bound(vma);
    if (it == mmap_backing.begin())
        return make_pair(nullptr, 0);
    --it;
    if (vma - it->first >= it->second.size)
        return make_pair(nullptr, 0);
    return make_pair(it->second.filename,
                     vma - it->first + it->second.file_offset);
}