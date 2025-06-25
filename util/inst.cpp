// SPDX-License-Identifier: Apache 2.0
/*
 * inst.cpp - Instruction abstraction
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <cassert>
#include <error.h>
#include <capstone.h>
#include <libnexus-rv/error.h>
#include <libnexus-rv/inst-helper.h>
#include "inst.h"

using namespace std;

struct auto_csh {
    auto_csh(csh handle) : handle(handle) {}
    ~auto_csh() {
        cs_close(&handle);
    }
    csh handle;
};

map<pair<cs_arch, cs_mode>, auto_csh> cached_csh;

uint64_t rv_inst_block::retire(nexusrv_trace_decoder *decoder) {
    check_exc(decoder);
    return addr + icnt * 2;
}

unsigned rv_inst_block::check_exc(nexusrv_trace_decoder *decoder) {
    unsigned event;
    int32_t retired = nexusrv_trace_try_retire(decoder, icnt, &event);
    if (retired < 0)
        throw rv_inst_exc_failed{retired};
    if ((uint32_t)retired < icnt) {
        switch (event) {
            // Expect these messages at any given i-cnt
            case NEXUSRV_Trace_Event_Trap:
            case NEXUSRV_Trace_Event_DirectSync:
            case NEXUSRV_Trace_Event_IndirectSync:
            case NEXUSRV_Trace_Event_Sync:
            case NEXUSRV_Trace_Event_Error:
                throw rv_inst_exc_event{addr + retired * 2, event};
        }
        error(-1, 0,
              "Expecting trap/sync/error, but got %s, icnt=%" PRIi32,
              str_nexusrv_trace_event(event), retired);
    }
    return event;
}

rv_ib_dir_jmp::rv_ib_dir_jmp(shared_ptr<memory_view> vm,
                             uint64_t addr, uint32_t icnt,
                             auto_cs_insn insn) :
        rv_inst_block(vm, addr, icnt, move(insn)) {
    auto *rv_detail = &this->insn->detail->riscv;
    assert(rv_detail->op_count > 0);
    auto op_last = rv_detail->op_count - 1;
    assert(rv_detail->operands[op_last].type == RISCV_OP_IMM);
    off = rv_detail->operands[op_last].imm;
}

uint64_t rv_ib_dir_jmp::retire(nexusrv_trace_decoder *decoder) {
    check_exc(decoder);
    return addr + icnt * 2 - insn->size + off;
}

uint64_t rv_ib_cond_branch::retire(nexusrv_trace_decoder *decoder) {
    check_exc(decoder);
    int tnt = nexusrv_trace_next_tnt(decoder);
    if (tnt < 0)
        throw rv_inst_exc_failed{tnt};
    taken = !!tnt;
    if (!taken)
        return addr + icnt * 2;
    return addr + icnt * 2 - insn->size + off;
}

int rv_ib_cond_branch::print(FILE *fp) const {
    return rv_inst_block::print(fp) +
        fprintf(fp, "%s", taken ? " [taken]" : "");
}

string rv_ib_cond_branch::to_string() const {
    return cppfmt("%s %s%s",
        insn->mnemonic, insn->op_str,
        taken ? " [taken]" : "");
}

uint64_t rv_ib_dir_call::retire(nexusrv_trace_decoder *decoder) {
    uint64_t target = rv_ib_dir_jmp::retire(decoder);
    stack0 = nexusrv_trace_callstack_used(decoder);
    nexusrv_trace_push_call(decoder, addr + icnt * 2);
    stack1 = nexusrv_trace_callstack_used(decoder);
    return target;
}

int rv_ib_dir_call::print(FILE *fp) const {
    return rv_inst_block::print(fp) +
        fprintf(fp, " [stack:%u->%u]", stack0, stack1);
}

string rv_ib_dir_call::to_string() const {
    return cppfmt("%s %s [stack:%u->%u]",
        insn->mnemonic, insn->op_str,
        stack0, stack1);
}

uint64_t rv_ib_indir_jmp::retire(nexusrv_trace_decoder *decoder) {
    auto event = check_exc(decoder);
    if (event != NEXUSRV_Trace_Event_Indirect)
        throw rv_inst_exc_failed{-nexus_trace_mismatch};
    nexusrv_trace_indirect indir;
    int rc = nexusrv_trace_next_indirect(decoder, &indir);
    if (rc < 0)
        throw rv_inst_exc_failed{rc};
    return indir.target;
}

uint64_t rv_ib_indir_call::retire(nexusrv_trace_decoder *decoder) {
    uint64_t target = rv_ib_indir_jmp::retire(decoder);
    stack0 = nexusrv_trace_callstack_used(decoder);
    nexusrv_trace_push_call(decoder, addr + icnt * 2);
    stack1 = nexusrv_trace_callstack_used(decoder);
    return target;
}

int rv_ib_indir_call::print(FILE *fp) const {
    return rv_inst_block::print(fp) +
        fprintf(fp, " [stack:%u->%u]", stack0, stack1);
}

string rv_ib_indir_call::to_string() const {
    return cppfmt("%s %s [stack:%u->%u]",
        insn->mnemonic, insn->op_str,
        stack0, stack1);
}

uint64_t rv_ib_ret::retire(nexusrv_trace_decoder *decoder) {
    auto event = check_exc(decoder);
    stacksz = nexusrv_trace_callstack_used(decoder);
    IRO = false;
    if (event == NEXUSRV_Trace_Event_Indirect) {
        nexusrv_trace_indirect indir;
        int rc = nexusrv_trace_next_indirect(decoder, &indir);
        if (rc < 0)
            throw rv_inst_exc_failed{rc};
        return indir.target;
    }
    uint64_t target;
    int rc = nexusrv_trace_pop_ret(decoder, &target);
    if (rc < 0)
        throw rv_inst_exc_failed{rc};
    IRO = true;
    return target;
}

int rv_ib_ret::print(FILE *fp) const {
    return rv_inst_block::print(fp) +
        fprintf(fp, "%s [stack:%u->%u]",
                IRO ? " [implicit]" : " [explicit]",
                stacksz, IRO ? stacksz - 1 : stacksz);
}

string rv_ib_ret::to_string() const {
    return cppfmt("%s %s%s [stack:%u->%u]",
        insn->mnemonic, insn->op_str,
        IRO ? " [implicit]" : " [explicit]",
        stacksz, IRO ? stacksz - 1 : stacksz);
}

uint64_t rv_ib_co_swap::retire(nexusrv_trace_decoder *decoder) {
    stack0 = nexusrv_trace_callstack_used(decoder);
    IRO = false;
    coswap = false;
    uint64_t target;
    if (decoder->msg_decoder->hw_cfg->quirk_sifive)
        target = rv_ib_indir_call::retire(decoder);
    else {
        coswap = true;
        target = rv_ib_ret::retire(decoder);
        nexusrv_trace_push_call(decoder, addr + icnt * 2);
    }
    stack1 = nexusrv_trace_callstack_used(decoder);
    return target;
}

int rv_ib_co_swap::print(FILE *fp) const {
    return rv_inst_block::print(fp) +
        fprintf(fp, "%s%s [stack:%u->%u]",
                IRO ? " [implicit]" : " [explicit]",
                coswap ? " [coswap]" : "",
                stack0, stack1);
}

string rv_ib_co_swap::to_string() const {
    return cppfmt("%s %s%s%s [stack:%u->%u]",
        insn->mnemonic, insn->op_str,
        IRO ? " [implicit]" : " [explicit]",
        coswap ? " [coswap]" : "",
        stack0, stack1);
}

uint64_t rv_ib_int::retire(nexusrv_trace_decoder *decoder) {
    unsigned event;
    /* Expect an exception exactly at the start of the insn */
    int32_t retired = nexusrv_trace_try_retire(
                    decoder, icnt - insn->size / 2, &event);
    if (retired < 0)
        throw rv_inst_exc_failed{retired};
    if ((uint32_t)retired < icnt - insn->size / 2)
        throw rv_inst_exc_event{addr + retired * 2, event};
    if (event != NEXUSRV_Trace_Event_Trap)
        throw rv_inst_exc_failed{-nexus_trace_mismatch};
    nexusrv_trace_indirect indir;
    int rc = nexusrv_trace_next_indirect(decoder, &indir);
    if (rc < 0)
        throw rv_inst_exc_failed{rc};
    return indir.target;
}

static csh get_csh(cs_arch arch, cs_mode mode) {
    auto it = cached_csh.find(make_pair(arch, mode));
    if (it != cached_csh.end())
        return it->second.handle;
    csh handle;
    if (cs_open(arch, mode, &handle) != CS_ERR_OK)
        error(-1, 0, "failed to open csh for arch %d mode %d", arch, mode);
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
    cached_csh.emplace(make_pair(arch, mode), handle);
    return handle;
}

auto_cs_insn rv_inst_block::disasm1(shared_ptr<memory_view> vm, uint64_t va, bool rv64) {
    auto [mapped, len] = vm->try_map(va);
    if (!mapped)
        return auto_cs_insn(nullptr, &cs_free1);
    auto cs_handle = get_csh(CS_ARCH_RISCV,
                             rv64 ?
                             (cs_mode)(CS_MODE_RISCV64 | CS_MODE_RISCVC) :
                             (cs_mode)(CS_MODE_RISCV32 | CS_MODE_RISCVC));
    cs_insn *insn = nullptr;
    if (!cs_disasm(cs_handle, mapped, len, va, 1, &insn))
        error(-1, 0, "failed to decode insn @%" PRIx64 " len=%zu err=%d",
              va, len, cs_errno(cs_handle));
    return auto_cs_insn(insn, &cs_free1);
}

shared_ptr<rv_inst_block> rv_inst_block::fetch(shared_ptr<memory_view> vm, uint64_t va, bool rv64) {
    auto [mapped, len] = vm->try_map(va);
    auto orig_va = va;
    auto orig_mapped = mapped;
    auto orig_len = len;
    if (!mapped)
        return nullptr;
    auto cs_handle = get_csh(CS_ARCH_RISCV,
                             rv64 ?
                             (cs_mode)(CS_MODE_RISCV64 | CS_MODE_RISCVC) :
                             (cs_mode)(CS_MODE_RISCV32 | CS_MODE_RISCVC));
    auto_cs_insn insn(cs_malloc(cs_handle), &cs_free1);
    while (len >= 2) {
        if (!cs_disasm_iter(cs_handle, &mapped, &len, &va, insn.get())) {
            // Ignore instructions capstone can't handle for now (such as bitmanip)
            uint8_t b = *mapped;
            size_t inst_len = ((b & 3) == 3) ? 4 : 2;
            if (len < inst_len)
                error(-1, 0, "partial instruction");
            va += inst_len;
            mapped += inst_len;
            len -= inst_len;
        }
        assert(mapped == orig_mapped + (orig_len - len));
        assert((mapped - orig_mapped) % 2 == 0);
        auto icnt = (mapped - orig_mapped) / 2;
        auto detail = insn->detail;
        for (unsigned i = 0 ; i < detail->groups_count; ++i) {
            if (detail->groups[i] == RISCV_GRP_BRANCH_RELATIVE)
                return make_shared<rv_ib_cond_branch>(vm, orig_va, icnt, move(insn));
        }
        switch (insn->id) {
            case RISCV_INS_MRET:
            case RISCV_INS_SRET:
            case RISCV_INS_URET:
                return make_shared<rv_ib_iret>(vm, orig_va, icnt, move(insn));
            case RISCV_INS_ECALL:
            case RISCV_INS_EBREAK:
            case RISCV_INS_C_EBREAK:
                return make_shared<rv_ib_int>(vm, orig_va, icnt, move(insn));
        }
        auto *rv_detail = &insn->detail->riscv;
        unsigned itype = NEXUSRV_ITYPE_None;
        auto lastop = rv_detail->op_count - 1;
        switch (insn->id) {
            case RISCV_INS_JAL:
                switch (rv_detail->op_count) {
                    default:
                        assert(false);
                    case 1:
                        // First operand is omitted:
                        //   jal <label> -> jal ra, <label>
                        //   j   <label> -> jal x0, <label>
                        if (!strcmp(insn->mnemonic, "j"))
                            itype = nexusrv_inst_jal_types(0);
                        else if (!strcmp(insn->mnemonic, "jal"))
                            itype = nexusrv_inst_jal_types(
                                    RISCV_REG_RA - RISCV_REG_ZERO);
                        else
                            assert(false);
                        break;
                    case 2:
                        assert(!strcmp(insn->mnemonic, "jal"));
                        assert(rv_detail->operands[0].type == RISCV_OP_REG);
                        itype = nexusrv_inst_jal_types(
                                rv_detail->operands[0].reg - RISCV_REG_ZERO);
                        break;
                }
                assert(rv_detail->operands[lastop].type == RISCV_OP_IMM);
                break;
            case RISCV_INS_JALR:
                switch (rv_detail->op_count) {
                    default:
                        assert(false);
                    case 1:
                        assert(rv_detail->operands[0].type == RISCV_OP_REG);
                        if (!strcmp(insn->mnemonic, "jr"))
                            itype = nexusrv_inst_jalr_types(
                                    0,
                                    rv_detail->operands[0].reg - RISCV_REG_ZERO);
                        else if (!strcmp(insn->mnemonic, "jalr"))
                            itype = nexusrv_inst_jalr_types(
                                    RISCV_REG_RA - RISCV_REG_ZERO,
                                    rv_detail->operands[0].reg - RISCV_REG_ZERO);
                        else
                            assert(false);
                        break;
                    case 2:
                        assert(!strcmp(insn->mnemonic, "jalr"));
                        assert(rv_detail->operands[0].type == RISCV_OP_REG);
                        assert(rv_detail->operands[1].type == RISCV_OP_IMM);
                        itype = nexusrv_inst_jalr_types(
                                RISCV_REG_RA - RISCV_REG_ZERO,
                                rv_detail->operands[0].reg - RISCV_REG_ZERO);
                        break;
                    case 3:
                        assert(!strcmp(insn->mnemonic, "jalr"));
                        assert(rv_detail->operands[0].type == RISCV_OP_REG);
                        assert(rv_detail->operands[1].type == RISCV_OP_REG);
                        assert(rv_detail->operands[2].type == RISCV_OP_IMM);
                        itype = nexusrv_inst_jalr_types(
                                rv_detail->operands[0].reg - RISCV_REG_ZERO,
                                rv_detail->operands[1].reg - RISCV_REG_ZERO);
                        break;
                }
                break;
            case RISCV_INS_C_J:
                assert(!strcmp(insn->mnemonic, "c.j"));
                assert(rv_detail->op_count == 1);
                assert(rv_detail->operands[0].type == RISCV_OP_IMM);
                itype = NEXUSRV_ITYPE_Direct_Jump;
                break;
            case RISCV_INS_C_JR:
                assert(!strcmp(insn->mnemonic, "c.jr"));
                assert(rv_detail->op_count == 1);
                assert(rv_detail->operands[0].type == RISCV_OP_REG);
                itype = nexusrv_inst_cjr_types(
                        rv_detail->operands[0].reg - RISCV_REG_ZERO);
                break;
            case RISCV_INS_C_JAL:
                assert(!strcmp(insn->mnemonic, "c.jal"));
                assert(rv_detail->op_count == 1);
                assert(rv_detail->operands[0].type == RISCV_OP_IMM);
                itype = NEXUSRV_ITYPE_Direct_Call;
                break;
            case RISCV_INS_C_JALR:
                assert(!strcmp(insn->mnemonic, "c.jalr"));
                assert(rv_detail->op_count == 1);
                assert(rv_detail->operands[0].type == RISCV_OP_REG);
                itype = nexusrv_inst_cjalr_types(
                        rv_detail->operands[0].reg - RISCV_REG_ZERO);
                break;
        }
        switch (itype) {
            case NEXUSRV_ITYPE_Direct_Jump:
                return make_shared<rv_ib_dir_jmp>(vm, orig_va, icnt, move(insn));
            case NEXUSRV_ITYPE_Direct_Call:
                return make_shared<rv_ib_dir_call>(vm, orig_va, icnt, move(insn));
            case NEXUSRV_ITYPE_Indirect_Jump:
                return make_shared<rv_ib_indir_jmp>(vm, orig_va, icnt, move(insn));
            case NEXUSRV_ITYPE_Indirect_Call:
                return make_shared<rv_ib_indir_call>(vm, orig_va, icnt, move(insn));
            case NEXUSRV_ITYPE_Coroutine_Swap:
                return make_shared<rv_ib_co_swap>(vm, orig_va, icnt, move(insn));
            case NEXUSRV_ITYPE_Function_Return:
                return make_shared<rv_ib_ret>(vm, orig_va, icnt, move(insn));
        }
    }
    return nullptr;
}