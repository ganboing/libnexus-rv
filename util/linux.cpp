// SPDX-License-Identifier: Apache 2.0
/*
 * linux.cpp - Linux corefile and obj file store
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <bfd.h>
#include <error.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <fcntl.h>
#include "linux.h"
#include "sym.h"

using namespace std;
using namespace std::filesystem;

static set<string_view> ignored_mod_sections = {
        ".strtab"sv,
        ".symtab"sv,
};

static const string str_kernel("kernel");

shared_ptr<obj_file> linux_file_store::search_in(const vector<string>& dirs,
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

shared_ptr<obj_file> linux_file_store::get(const char *filename) const {
    return search_in(prefixes, filename, bfd_object);
}

shared_ptr<obj_file> linux_file_store::get_dbg(const char *filename) const {
    char filename_dbg[strlen(filename) +
        1 /* slash */ + 6 /* .debug */ + 1 /* null */];
    size_t n = snprintf(filename_dbg, sizeof(filename_dbg),
                        "%s/.debug", filename);
    assert(n + 1 == sizeof(filename_dbg));
    // try /usr/bin/ls.debug
    auto obj = search_in(prefixes, filename_dbg, bfd_object);
    if (obj)
        return obj;
    // /usr/lib/debug/usr/bin/ls.debug
    return search_in(dbg_prefixes, filename_dbg, bfd_object);
}

shared_ptr<obj_file> linux_file_store::get_dbg_buildid(
        const vector<uint8_t>& buildid) const {
    if (buildid.size() < 2)
        error(-1, 0, "buildid is too short");
    static const char buildid_prefix[] = ".build-id/";
    char filename_dbg[buildid.size() * 2 +
            sizeof(buildid_prefix) + 1 /* slash */ + 6 /* .debug */];
    strcpy(filename_dbg, buildid_prefix);
    char *p = filename_dbg + sizeof(buildid_prefix) - 1;
    bin2hex(p, buildid.data(), 1);
    p += 2;
    *p++ = '/';
    bin2hex(p, buildid.data() + 1, buildid.size() - 1);
    p += 2 * (buildid.size() - 1);
    strcpy(p, ".debug");
    p += sizeof(".debug");
    assert(p == &filename_dbg[sizeof(filename_dbg)]);
    return search_in(dbg_prefixes, filename_dbg, bfd_object);
}

linux_kmods_file_store::linux_kmods_file_store(vector<string> prefixes,
                                               vector<string> dbg_prefixes,
                                               string kernel_version):
           linux_file_store(move(prefixes), move(dbg_prefixes)),
           kernel_version(move(kernel_version)) {
    for (auto &d : this->prefixes) {
        auto full_path = filesystem::path(d)
                / "lib" / "modules" / this->kernel_version / "modules.dep";
        error_code ec;
        if (!filesystem::exists(full_path, ec))
            continue;
        moddep = parse_moddep(full_path);
        return;
    }
    error(0, 0, "failed to load modules.dep, no kernel module information");
}

shared_ptr<obj_file> linux_kmods_file_store::get(const char *mod) const {
    if (!strcmp(mod, "kernel")) {
        auto vmlinux_path = path("lib") / "modules" / kernel_version / "vmlinux";
        return search_in(dbg_prefixes, vmlinux_path.c_str(),bfd_object);
    }
    auto it = moddep.find(mod);
    if (it == moddep.end()) {
        error(0, 0, "unrecognized kernel module %s", mod);
        return nullptr;
    }
    auto mod_path = path("lib") / "modules" / kernel_version / it->second;
    return search_in(prefixes, mod_path.c_str(), bfd_object);
}

shared_ptr<obj_file> linux_kmods_file_store::get_dbg(const char *mod) const {
    if (!strcmp(mod, "kernel")) {
        auto vmlinux_path = path("lib") / "modules" / kernel_version / "vmlinux";
        return search_in(dbg_prefixes, vmlinux_path.c_str(),bfd_object);
    }
    auto it = moddep.find(mod);
    if (it == moddep.end()) {
        error(0, 0, "unrecognized kernel module %s", mod);
        return nullptr;
    }
    auto mod_path = path("lib") / "modules" / kernel_version /
            cppfmt("%s.debug", it->second.c_str());
    return search_in(dbg_prefixes, mod_path.c_str(), bfd_object);
}

map<string, string> linux_kmods_file_store::parse_moddep(const path& moddep) {
    map<string, string> ret;
    auto_file fp(fopen(moddep.c_str(), "r"), &fclose);
    if (!fp)
        error(-1, 0, "failed to open moddep %s", moddep.c_str());
    char *line = nullptr;
    size_t sz = 0;
    ssize_t rc;
    while ((rc = getline(&line, &sz, fp.get())) >= 0) {
        auto dep = strchr(line, ':');
        if (!dep)
            error(-1, 0, "invalid module.dep line %s", line);
        *dep++ = '\0';
        auto name = strrchr(line, '/');
        name = name ? name + 1 : line;
        auto suffix = strchr(name, '.');
        if (!suffix)
            goto wrong_suffix;
        if (strncmp(suffix, ".ko", 3))
            goto wrong_suffix;
        suffix[3] = '\0';
        string mod_path(line);
        suffix[0] = '\0';
        auto [it, inserted] = ret.emplace(name, move(mod_path));
        if (!inserted)
            error(-1, 0, "duplicate module %s", name);
    }
    free(line);
    return ret;
wrong_suffix:
    error(-1, 0, "invalid suffix, module %s", line);
    __builtin_unreachable();
}

template<typename INTTYPE>
struct linux_ucore_mapping {
    INTTYPE count;
    INTTYPE page_size;
    struct {
        INTTYPE start;
        INTTYPE end;
        INTTYPE file_ofs;
    } files[];
};

template<typename INTTYPE>
void parse_ucore_metadata(linux_ucore *core, const vector<uint8_t>& sec_data) {
    const linux_ucore_mapping<INTTYPE> *mapping =
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
                mapping->files[i].file_ofs * mapping->page_size,
                nullptr /* set file name later */});
        if (!inserted)
            error(-1, 0, "duplicate file backed mmap at %" PRIx64
                " filename %s", it->first, it->second.filename->c_str());
        auto nullbyte = find(filenames, end, '\0');
        if (nullbyte == end)
            error(1, 0, "Can not find next file name");
        it->second.filename = &*core->file_names.emplace(filenames).first;
        filenames = nullbyte + 1;
    }
}

linux_ucore::linux_ucore(const char *filename,
                         vector<string> sysroot_dirs, vector<string> dbg_dirs) :
        linux_core_file(filename, move(sysroot_dirs), move(dbg_dirs))
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
            parse_ucore_metadata<uint32_t>(this, sec_data);
            break;
        case bfd_mach_riscv64:
            parse_ucore_metadata<uint64_t>(this, sec_data);
            break;
        default:
            error(-1, 0,"unknown march %lu", march);
    }
}

tuple<const std::string*, uint64_t>
linux_ucore::get_file_backing(uint64_t vma) {
    auto it = mmap_backing.upper_bound(vma);
    if (it == mmap_backing.begin())
        return no_map;
    --it;
    if (vma - it->first >= it->second.size)
        return no_map;
    return make_tuple(it->second.filename,
                     vma - it->first + it->second.file_offset);
}

pair<string, string> parse_kernel_ver(const string &ver) {
    static const string_view ver_prefix = "Linux version "sv;
    if (!ver.starts_with(ver_prefix))
        error(-1, 0, "Unexpected kernel ver %s", ver.c_str());
    auto it = ver.begin() + ver_prefix.size();
    auto it2 = find(it, ver.end(), ' ');
    if (it2 == ver.end())
        return make_pair(string(it, ver.end()), string());
    return make_pair(string(it, it2), string(it2 + 1, ver.end()));
}

linux_kcore::linux_kcore(const char *procfs,
                         const char *sysfs,
                         vector<string> sysroot_dirs,
                         vector<string> dbg_dirs) :
        linux_core_file(cppfmt("%s/kcore", procfs).c_str()) {
    vmlinux_start = vmlinux_end = bpf_start = kaslr_offset = 0;
    auto_file fkallsyms(fopen(cppfmt(
            "%s/kallsyms", procfs).c_str(), "r"), &fclose);
    if (!fkallsyms)
        error(-1, 0, "unable to open kallsyms, linux_kcore cannot continue");
    sym_iterate_kallsyms(fkallsyms.get(), [&](
            uint64_t addr, char type, const char *sym, const char* mod) {
        if (!vmlinux_start && type == 'T' && !strcmp(sym, "_start"))
            vmlinux_start = addr;
        if (!vmlinux_end && type == 'B' && !strcmp(sym, "_end"))
            vmlinux_end = addr;
        if (mod && !strcmp(mod, "[bpf]") && (!bpf_start || bpf_start > addr))
            bpf_start = addr;
        auto modp = mod ? &*kallsyms_mods.emplace(mod).first : nullptr;
        kallsyms.emplace(addr, make_tuple(string(sym), type,modp));
        return true;
    });
    if (!vmlinux_start || !vmlinux_end)
        error(-1, 0, "failed to identify vmlinux_start/end");
    auto [kernel_version, release_str] = parse_kernel_ver(read_file(
            cppfmt("%s/version", procfs).c_str()));
    kernel_release = move(release_str);
    store = make_shared<linux_kmods_file_store>(
            move(sysroot_dirs), move(dbg_dirs), move(kernel_version));
    auto vmlinux = store->get(str_kernel.c_str());
    if (vmlinux) {
        // Takes care of KASLR offset
        auto &sections = vmlinux->get_sec_by_name();
        auto it = sections.find(".head.text"sv);
        if (it != sections.end())
            kaslr_offset = vmlinux_start - it->second->vma;
    }
    directory_iterator di(cppfmt("%s/module", sysfs));
    for (auto& dentry : di) {
        auto& path = dentry.path();
        auto mod_it = mod_sections.emplace(path.filename().string(),
                                           map<string, uint64_t>{}).first;
        auto section_dir = path / "sections";
        error_code ec;
        if (!is_directory(section_dir, ec))
            continue;
        auto buildid_path = path / "notes" / ".note.gnu.build-id";
        if (exists(buildid_path, ec))
            mod_buildids.emplace(mod_it->first,
                                 read_file_binary(buildid_path.c_str()));
        for (auto& dentry : directory_iterator(section_dir)) {
            auto& path = dentry.path();
            auto sec_name = path.filename().string();
            if (sec_name.starts_with(".init."sv) ||
                sec_name.starts_with(".note."sv) ||
                ignored_mod_sections.count(sec_name))
                continue;
            uint64_t addr = strtoull(read_file(path.c_str()).c_str(),
                                     nullptr, 16);
            auto section_it = mod_it->second.emplace(
                    move(sec_name), addr).first;
            auto [it, inserted] = sections_map.emplace(addr,
                 make_pair(&mod_it->first,&section_it->first));
            if (!inserted)
                error(-1, 0,
                      "overlapping section @%" PRIx64 " %s:%s vs. %s:%s", addr,
                    mod_it->first.c_str(), section_it->first.c_str(),
                    it->second.first->c_str(), it->second.second->c_str());
        }
    }
}

tuple<const std::string*, const std::string*, uint64_t>
linux_kcore::get_file_vma(uint64_t vma) {
    if (vma >= vmlinux_start && vma < vmlinux_end) {
        return make_tuple(&str_kernel, nullptr, vma - kaslr_offset);
    }
    // modules/BPF are 2GB below vmlinux
    auto mod_start = vmlinux_start - (2ULL << 30);
    if (vma < mod_start || vma >= vmlinux_end)
        return no_map_or_sym;
    if (bpf_start && vma >= bpf_start)
        return no_map_or_sym;
    auto it = sections_map.upper_bound(vma);
    if (it == sections_map.begin())
        return no_map_or_sym;
    --it;
    return make_tuple(it->second.first,
                      it->second.second,
                      vma - it->first);
}

static int compare_sym_type(char l, char r) {
    return int(isupper(l)) - int(isupper(r));
}

tuple<const std::string*, const std::string*, uint64_t>
linux_kcore::get_label(uint64_t vma) {
    auto it = kallsyms.upper_bound(vma);
    optional<decltype(it)> last;
    while (it != kallsyms.begin()) {
        --it;
        if (!last.has_value()) {
            last.emplace(it);
            continue;
        }
        if (it->first < (*last)->first)
            break;
        if (compare_sym_type(get<1>(it->second),
                             get<1>((*last)->second)) > 0)
            last.emplace(it);
    }
    if (!last.has_value())
        return no_map_or_sym;
    return make_tuple(
            &get<0>((*last)->second),
            get<2>((*last)->second),
            (*last)->first);
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