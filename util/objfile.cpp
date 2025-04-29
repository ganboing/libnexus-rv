#include <error.h>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>
#include "objfile.h"

using namespace std;

static int open_bfd(const char *filename) {
    int fd = open(filename, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        error(-1, errno, "failed to open %s", filename);
    return fd;
}

obj_file::obj_file(const char *filename, bfd_format format) :
autofd(open_bfd(filename)),
abfd(bfd_fdopenr(filename, nullptr, autofd.fd), &bfd_close)
{
    if (!abfd)
        error(-1, 0, "failed to open %s", filename);
    if (!bfd_check_format(abfd.get(), format))
        error(-1, 0, "failed to bfd_check_format");
    bfd_map_over_sections(abfd.get(), [](bfd *abfd, asection *sect, void *obj) {
        auto objf = static_cast<obj_file*>(obj);
        objf->sect_by_name.emplace(string_view(sect->name), sect);
        if (!sect->size)
            return;
        if ((sect->flags & (SEC_LOAD | SEC_ALLOC)) != (SEC_LOAD | SEC_ALLOC))
            return;
        auto [it_vma, inserted_vma] =
                objf->sect_by_vma.emplace(sect->vma, sect);
        if (!inserted_vma)
            error(-1, 0, "vma overlapping section %s vs. %s",
                  it_vma->second->name, sect->name);
        auto [it_filepos, inserted_filepos] =
                objf->sect_by_fileoff.emplace(sect->filepos, sect);
        if (!inserted_filepos)
            error(-1, 0, "filepos overlapping section %s vs. %s",
                  it_filepos->second->name, sect->name);
    }, this);
}

pair<void*, size_t> obj_file::mmap(uint64_t fileoff, int prot, int flags) {
    auto it = sect_by_fileoff.find(fileoff);
    if (it == sect_by_fileoff.end())
        error(-1, 0, "fileoff %" PRIx64 "not found", fileoff);
    auto it2 = sect_mapped.find(fileoff);
    auto *asection = it->second;
    size_t sz = asection->rawsize;
    if (!sz)
        sz = asection->size;
    assert(sz);
    if (it2 != sect_mapped.end())
        return make_pair(it2->second, sz);
    size_t pagesz = getpagesize(), pagemask = pagesz - 1;
    off_t off = asection->filepos;
    off_t pg_offset = off & ~pagemask;
    size_t pg_len = (off - pg_offset + sz + pagemask) & ~pagemask;
    void *ret_addr = nullptr;
    size_t ret_size = 0;
    void *addr = bfd_mmap(abfd.get(), nullptr, pg_len, prot, flags, pg_offset,
                          &ret_addr, &ret_size);
    if (addr == MAP_FAILED)
        return make_pair(nullptr, 0);
    assert(ret_addr == addr);
    assert(ret_size == pg_len);
    it2 = sect_mapped.emplace(fileoff, (char*)addr + off - pg_offset).first;
    return make_pair(it2->second, sz);
}

void obj_file::unmap(uint64_t fileoff) {
    auto node = sect_mapped.extract(fileoff);
    if (node.empty())
        return;
    auto it = sect_by_fileoff.find(fileoff);
    if (it == sect_by_fileoff.end())
        error(-1, 0, "fileoff %" PRIx64 "not found", fileoff);
    auto *asection = it->second;
    size_t sz = asection->rawsize;
    if (!sz)
        sz = asection->size;
    assert(sz);
    size_t pagesz = getpagesize(), pagemask = pagesz - 1;
    off_t off = asection->filepos;
    off_t pg_offset = off & ~pagemask;
    size_t pg_len = (off - pg_offset + sz + pagemask) & ~pagemask;
    munmap(node.mapped(), pg_len);
}

uint64_t obj_file::fileoff_to_va(uint64_t offset) const {
    auto it = sect_by_fileoff.upper_bound(offset);
    if (it == sect_by_fileoff.begin())
        return 0;
    --it;
    if (offset - it->first >= it->second->size)
        return 0;
    return offset - it->first + it->second->vma;
}

optional<vector<uint8_t> > obj_file::get_build_id() const {
    struct {
        uint32_t name_sz;
        uint32_t desc_sz;
        uint32_t type;
        uint8_t data[];
    } gnu_build_id;
    auto it = sect_by_name.find(".note.gnu.build-id"sv);
    if (it == sect_by_name.end())
        return nullopt;
    vector<uint8_t> sec_data;
    sec_data.resize(bfd_section_size(it->second));
    if (sec_data.size() < sizeof(gnu_build_id))
        error(-1, 0, "build-id too short");
    if (!bfd_get_section_contents(get_bfd(), it->second,
                                  sec_data.data(),
                                  0, sec_data.size()))
        error(-1, 0, "failed to get section data");
    memcpy(&gnu_build_id, sec_data.data(), sizeof(gnu_build_id));
    if (sec_data.size() < sizeof(gnu_build_id) + gnu_build_id.name_sz)
        error(-1, 0, "build-id too short");
    if (gnu_build_id.name_sz != sizeof("GNU") || gnu_build_id.type != 3 ||
        strcmp((char*)&sec_data[sizeof(gnu_build_id)], "GNU"))
        error(-1, 0, "unknown build-id type");
    return vector<uint8_t>(sec_data.begin() +
            sizeof(gnu_build_id) + gnu_build_id.name_sz, sec_data.end());
}

obj_file::~obj_file() {
    auto it = sect_mapped.begin();
    for (auto i = sect_mapped.begin(), j = sect_mapped.end(); i != j;) {
        auto fileoff = i++->first;
        unmap(fileoff);
    }
}

shared_ptr<obj_file> obj_file_store::search_in(const vector<string>& dirs,
                                               const char *filename,
                                               bfd_format format) {
    error_code ec;
    // Convert to relative path
    while (*filename == '/')
        ++filename;
    for (auto &d : dirs) {
        auto full_path = filesystem::path(d) / filename;
        if (!filesystem::exists(full_path, ec))
            continue;
        return make_shared<obj_file>(full_path.c_str(), format);
    }
    return nullptr;
}

shared_ptr<obj_file> obj_file_store::get(const char *filename, bfd_format format) const {
    return search_in(prefixes, filename, format);
}

shared_ptr<obj_file> obj_file_store::get_dbg(const char *filename, bfd_format format) const {
    char filename_dbg[strlen(filename) + 1 /* slash */ + 6 /* .debug */ + 1 /* null */];
    size_t n = snprintf(filename_dbg, sizeof(filename_dbg), "%s/.debug", filename);
    assert(n + 1 == sizeof(filename_dbg));
    // try /usr/bin/ls.debug
    auto obj = search_in(prefixes, filename_dbg, format);
    if (obj)
        return obj;
    // /usr/lib/debug/usr/bin/ls.debug
    return search_in(dbg_prefixes, filename_dbg, format);
}

shared_ptr<obj_file> obj_file_store::get_dbg_buildid(const std::vector<uint8_t>& buildid,
                                                     bfd_format format) const {
    if (buildid.size() < 2)
        error(-1, 0, "buildid is too short");
    static const char buildid_prefix[] = ".build-id/";
    char filename_dbg[buildid.size() * 2 + sizeof(buildid_prefix) + 1 /* slash */ + 6 /* .debug */];
    strcpy(filename_dbg, buildid_prefix);
    char *p = filename_dbg + sizeof(buildid_prefix) - 1;
    bin2hex(p, buildid.data(), 1);
    p += 2;
    *p++ = '/';
    bin2hex(p, buildid.data() + 1, buildid.size() - 1);
    p += 2 * (buildid.size() - 1);
    strcpy(p, ".debug");
    p += sizeof(".debug");
    assert(p - filename_dbg == sizeof(filename_dbg));
    return search_in(dbg_prefixes, filename_dbg, format);
}

vector<string> split_dirs(const string& str) {
    vector<string> ret;
    for (auto i = str.begin(), j = str.end(); i != j; ++i) {
        auto brk = find(i, j, ':');
        if (i != brk)
            ret.emplace_back(i, brk);
        if (j == brk)
            break;
        i = brk;
    }
    return ret;
}