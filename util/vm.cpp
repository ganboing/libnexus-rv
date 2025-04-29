#include <error.h>
#include "vm.h"
#include "linux.h"

using namespace std;

void memory_view::load_core(shared_ptr<linuxcore> coredump) {
    for (auto& [vma, sect] : coredump->get_sect_by_vma()) {
        auto [sec_it, sec_inserted] = loaded_sections.emplace(vma,
                loaded_section{coredump, sect});
        if (!sec_inserted)
            error(-1, 0, "overlapping section at %" PRIx64 " %s vs %s",
                  vma, sec_it->second.asection->name, sect->name);
    }
}

pair<const uint8_t*, size_t> memory_view::try_map(uint64_t vma) {
    auto it = loaded_sections.upper_bound(vma);
    if (it == loaded_sections.begin())
        return make_pair(nullptr, 0);
    --it;
    if (vma - it->first >= it->second.asection->size)
        return make_pair(nullptr, 0);
    auto [addr, sz] = it->second.core->mmap(it->second.asection->filepos);
    if (!addr)
        error(-1, 0, "failed to map section %s", it->second.asection->name);
    if (vma - it->first >= sz)
        error(-1, 0, "trying to map beyond section limit");
    return make_pair(vma - it->first + (uint8_t*)addr, sz - (vma - it->first));
}

pair<shared_ptr<obj_file>, uint64_t> memory_view::query_sym(uint64_t vma) {
    auto it = loaded_sections.upper_bound(vma);
    if (it == loaded_sections.begin())
        return make_pair(nullptr, 0);
    --it;
    if (vma - it->first >= it->second.asection->size)
        return make_pair(nullptr, 0);
    auto [filename, fileoff] = it->second.core->get_file_backing(vma);
    if (!filename)
        return make_pair(nullptr, 0);
    auto it2 = backed_file_cache.find(filename);
    if (it2 == backed_file_cache.end()) {
        shared_ptr<obj_file> obj =  it->second.core->get_obj_store().get(filename), dbg;
        if (obj) {
            auto buildid = obj->get_build_id();
            // Try build-id if available
            if (buildid)
                dbg = it->second.core->get_obj_store().get_dbg_buildid(*buildid);
        }
        if (!dbg)
            dbg = it->second.core->get_obj_store().get_dbg(filename);
        it2 = backed_file_cache.emplace(filename, make_pair(obj, dbg)).first;
    }
    auto [bin, dbg] = it2->second;
    if (!bin)
        return make_pair(nullptr, 0);
    return make_pair(dbg ? dbg : bin, bin->fileoff_to_va(fileoff));
}