#include <error.h>
#include "vm.h"
#include "linux.h"

using namespace std;

void memory_view::load_core(shared_ptr<core_file> coredump) {
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

tuple<shared_ptr<obj_file>, const string*, uint64_t> memory_view::query_sym(uint64_t vma) {
    auto it = loaded_sections.upper_bound(vma);
    if (it == loaded_sections.begin())
        return no_map_or_sym;
    --it;
    if (vma - it->first >= it->second.asection->size)
        return no_map_or_sym;
    auto core = it->second.core;
    auto [fn1, fileoff] = core->get_file_backing(vma);
    auto [fn2, filesection, sectionvma] = core->get_file_vma(vma);
    // Use file offset if present
    auto filename = fn1 ? fn1 : fn2;
    if (!filename)
        return no_map_or_sym;
    auto& cache = backed_file_cache[core];
    auto it2 = cache.find(filename);
    if (it2 == cache.end()) {
        auto obj_store = core->obj_store();
        shared_ptr<obj_file> bin = obj_store->get(filename->c_str()), dbg = nullptr;
        if (bin) {
            auto buildid = bin->get_build_id();
            // Try build-id if available
            if (buildid)
                dbg = obj_store->get_dbg_buildid(*buildid);
        }
        if (!dbg)
            dbg = obj_store->get_dbg(filename->c_str());
        it2 = cache.emplace(filename, make_pair(bin, dbg)).first;
    }
    auto [bin, dbg] = it2->second;
    if (fn1) {
        // Use file offset if present
        // Binary must be found to produce the correct VMA from offset
        if (!bin)
            return no_map_or_sym;
        return make_tuple(dbg ? dbg : bin, nullptr, bin->fileoff_to_va(fileoff));
    }
    // If neither binary nor debug info is found, just return null
    if (!bin && !dbg)
        return no_map_or_sym;
    return make_tuple(dbg ? dbg : bin, filesection, sectionvma);
}

tuple<const string*, const string*, uint64_t> memory_view::query_label(uint64_t vma) {
    auto it = loaded_sections.upper_bound(vma);
    if (it == loaded_sections.begin())
        return no_map_or_sym;
    --it;
    if (vma - it->first >= it->second.asection->size)
        return no_map_or_sym;
    auto core = it->second.core;
    return core->get_label(vma);
}