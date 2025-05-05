#ifndef LIBNEXUS_RV_INST_H
#define LIBNEXUS_RV_INST_H

#include <memory>
#include <capstone.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <libnexus-rv/trace-decoder.h>
#ifdef __cplusplus
}
#endif
#include "vm.h"

static inline void cs_free1(cs_insn *insn) {
    cs_free(insn, 1);
}

typedef std::unique_ptr<cs_insn, decltype(&cs_free1)> auto_cs_insn;

struct rv_inst_block : std::enable_shared_from_this<rv_inst_block> {
    inline rv_inst_block(std::shared_ptr<memory_view> vm,
                         uint64_t addr, uint32_t icnt,
                         auto_cs_insn insn) :
    vm(vm), addr(addr), icnt(icnt), insn(std::move(insn)) {}
    inline virtual ~rv_inst_block() {};
    static std::shared_ptr<rv_inst_block> fetch(
            std::shared_ptr<memory_view> vm, uint64_t addr, bool rv64);
    static auto_cs_insn disasm1(
            std::shared_ptr<memory_view> vm, uint64_t addr, bool rv64);
    virtual uint64_t retire(nexusrv_trace_decoder *decoder);
    inline virtual int print(FILE* fp) {
        return fprintf(fp, "%s %s", insn->mnemonic, insn->op_str);
    }
protected:
    unsigned check_exc(nexusrv_trace_decoder *decoder);
    std::shared_ptr<memory_view> vm;
public:
    const uint64_t addr;
    const uint32_t icnt;
    const auto_cs_insn insn;
};

struct rv_inst_exc_event {
    uint64_t addr;
    unsigned event;
};

struct rv_inst_exc_failed {
    int rc;
};

struct rv_ib_dir_jmp : rv_inst_block {
    rv_ib_dir_jmp(std::shared_ptr<memory_view> vm,
                  uint64_t addr, uint32_t icnt,
                  auto_cs_insn insn);
    inline ~rv_ib_dir_jmp() {}
    uint64_t retire(nexusrv_trace_decoder *decoder) override;
protected:
    int64_t off;
};

struct rv_ib_cond_branch : rv_ib_dir_jmp {
    using rv_ib_dir_jmp::rv_ib_dir_jmp;
    inline ~rv_ib_cond_branch() {}
    uint64_t retire(nexusrv_trace_decoder *decoder) override;
    int print(FILE *fp) override;
protected:
    mutable bool taken;
};

struct rv_ib_dir_call : rv_ib_dir_jmp {
    using rv_ib_dir_jmp::rv_ib_dir_jmp;
    inline ~rv_ib_dir_call() {}
    uint64_t retire(nexusrv_trace_decoder *decoder) override;
    int print(FILE* fp) override;
protected:
    mutable unsigned stack0;
    mutable unsigned stack1;
};

struct rv_ib_indir_jmp : rv_inst_block {
    using rv_inst_block::rv_inst_block;
    inline ~rv_ib_indir_jmp() {}
    uint64_t retire(nexusrv_trace_decoder *decoder) override;
};

// Treat it the same as indiect jump for now (w/o context tracking)
struct rv_ib_iret : rv_ib_indir_jmp {
    using rv_ib_indir_jmp::rv_ib_indir_jmp;
    inline ~rv_ib_iret() {}
};

struct rv_ib_indir_call : virtual rv_ib_indir_jmp {
    using rv_ib_indir_jmp::rv_ib_indir_jmp;
    inline ~rv_ib_indir_call() {}
    uint64_t retire(nexusrv_trace_decoder *decoder) override;
    int print(FILE* fp) override;
protected:
    inline rv_ib_indir_call() : rv_ib_indir_jmp(
            nullptr, 0, 0, auto_cs_insn(nullptr, &cs_free1)) {}
    mutable unsigned stack0;
    mutable unsigned stack1;
};

struct rv_ib_ret : virtual rv_ib_indir_jmp {
    using rv_ib_indir_jmp::rv_ib_indir_jmp;
    inline ~rv_ib_ret() {}
    uint64_t retire(nexusrv_trace_decoder *decoder) override;
    int print(FILE* fp) override;
protected:
    inline rv_ib_ret() : rv_ib_indir_jmp(
            nullptr, 0, 0, auto_cs_insn(nullptr, &cs_free1)) {}
    mutable bool IRO;
    mutable unsigned stacksz;
};

struct rv_ib_co_swap : rv_ib_ret, rv_ib_indir_call {
    rv_ib_co_swap(std::shared_ptr<memory_view> vm,
                  uint64_t addr, uint32_t icnt,
                  auto_cs_insn insn) :
            rv_ib_indir_jmp(vm, addr, icnt, std::move(insn)) {}
    inline ~rv_ib_co_swap() {}
    uint64_t retire(nexusrv_trace_decoder *decoder) override;
    int print(FILE* fp) override;
protected:
    mutable bool coswap;
};

// Instruction block that raises exception/interrupt
struct rv_ib_int : rv_inst_block {
    using rv_inst_block::rv_inst_block;
    inline ~rv_ib_int() {}
    uint64_t retire(nexusrv_trace_decoder *decoder) override;
};

#endif
