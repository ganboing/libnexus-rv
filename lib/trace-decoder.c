// SPDX-License-Identifer: Apache 2.0
/*
 * trace-decoder.c - NexusRV trace decoder implementation
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <string.h>
#include <assert.h>
#include <libnexus-rv/error.h>
#include <libnexus-rv/trace-decoder.h>
#include <libnexus-rv/hist-array.h>

static const uint32_t MSG_ICNT_MAX = ((uint32_t)1 << 22) - 1;
static const uint32_t MSG_HREPEAT_MAX = ((uint32_t)1 << 18) - 1;

int nexusrv_trace_decoder_init(nexusrv_trace_decoder* decoder,
                               nexusrv_msg_decoder *msg_decoder) {
    memset(decoder, 0, sizeof(*decoder));
    decoder->msg_decoder = msg_decoder;
    decoder->res_hists = nexusrv_hist_array_new();
    if (!decoder->res_hists)
        return -nexus_no_mem;
    return 0;
}

void nexusrv_trace_decoder_fini(nexusrv_trace_decoder* decoder) {
    nexusrv_hist_array_free(decoder->res_hists);
}

static bool nexusrv_trace_check_msg(const nexusrv_msg *msg) {
    if (!nexusrv_msg_known(msg))
        return false;
    if (nexusrv_msg_has_icnt(msg) && msg->icnt > MSG_ICNT_MAX)
        return false;
    if (nexusrv_msg_has_hist(msg) && !msg->hist)
        return false;
    if (nexusrv_msg_has_hist(msg) && msg->hrepeat > MSG_HREPEAT_MAX)
        return false;
    return true;
}

static int nexusrv_trace_fetch_msg(nexusrv_trace_decoder *decoder) {
    if (decoder->msg_present)
        return 0;
    ssize_t rc = nexusrv_msg_decoder_next(decoder->msg_decoder, &decoder->msg);
    if (rc < 0)
        return rc;
    if (!rc)
        return -nexus_trace_eof;
    if (!nexusrv_trace_check_msg(&decoder->msg)) {
        nexusrv_msg_decoder_rewind_last(decoder->msg_decoder);
        return -nexus_msg_unsupported;
    }
    decoder->msg_present = 1;
    if (!nexusrv_msg_is_branch(&decoder->msg) || nexusrv_msg_is_sync(&decoder->msg))
        return 1;
    decoder->msg.hrepeat = 0;
    // Try to fetch next msg and see if we have a RepeatBranch
    nexusrv_msg msg2;
    rc = nexusrv_msg_decoder_next(decoder->msg_decoder, &msg2);
    if (rc < 0)
        return rc;
    if (msg2.tcode != NEXUSRV_TCODE_RepeatBranch) {
        nexusrv_msg_decoder_rewind_last(decoder->msg_decoder);
        return 1;
    }
    decoder->msg.hrepeat = msg2.hrepeat;
    return 1;
}

static void nexusrv_trace_retire_timestamp(nexusrv_trace_decoder *decoder,
                                           uint64_t *timestamp, uint32_t *hrepeat) {
    if (decoder->msg_decoder->hw_cfg->ext_sifive) {
        decoder->timestamp ^= *timestamp;
        *timestamp = 0;
        //XXX: hrepeat should be 0, but relax the check here
    } else {
        decoder->timestamp += *timestamp / (*hrepeat + 1);
        *timestamp -= *timestamp / (*hrepeat + 1);
    }
}

static uint32_t nexusrv_trace_available_tnts(nexusrv_trace_decoder *decoder) {
    uint32_t tnts = decoder->res_tnts;
    if (!decoder->msg_present)
        goto done_msg;
    assert(!nexusrv_msg_is_res(&decoder->msg));
    if (nexusrv_msg_has_hist(&decoder->msg))
        tnts += nexusrv_msg_hist_bits(decoder->msg.hist);
done_msg:
    assert(tnts >= decoder->consumed_tnts);
    return tnts - decoder->consumed_tnts;
}

static bool nexusrv_trace_consume_tnt(nexusrv_trace_decoder *decoder) {
    assert(nexusrv_trace_available_tnts(decoder));
    // Drain empty (timestamp reporting only) elements
    while (nexusrv_hist_array_size(decoder->res_hists)) {
        // Consume empty elements
        nexusrv_hist_arr_element *element =
                nexusrv_hist_array_front(decoder->res_hists);
        if (element->hist)
            break;
        assert(!element->hrepeat);
        nexusrv_trace_retire_timestamp(decoder, &element->timestamp, &element->hrepeat);
        nexusrv_hist_array_pop(decoder->res_hists);
    }
    if (!decoder->res_tnts) {
        assert(decoder->msg_present && nexusrv_msg_has_hist(&decoder->msg));
        unsigned hist_bits = nexusrv_msg_hist_bits(decoder->msg.hist);
        assert(hist_bits > decoder->consumed_tnts);
        ++decoder->consumed_tnts;
        // HIST bits goes from MSB -> LSB
        return (decoder->msg.hist &
            (1UL << (hist_bits - decoder->consumed_tnts))) != 0;
        // Do not retire the Msg, therefore, no need to reset consumed_tnts
    }
    assert(nexusrv_hist_array_size(decoder->res_hists));
    nexusrv_hist_arr_element *element =
            nexusrv_hist_array_front(decoder->res_hists);
    unsigned hist_bits = nexusrv_msg_hist_bits(element->hist);
    assert(hist_bits > decoder->consumed_tnts);
    ++decoder->consumed_tnts;
    // HIST bits goes from MSB -> LSB
    bool tnt = (element->hist &
            (1UL << (hist_bits - decoder->consumed_tnts))) != 0;
    if (hist_bits != decoder->consumed_tnts)
        return tnt;
    decoder->consumed_tnts = 0;
    nexusrv_trace_retire_timestamp(decoder, &element->timestamp, &element->hrepeat);
    assert(decoder->res_tnts >= hist_bits);
    decoder->res_tnts -= hist_bits;
    if (element->hrepeat)
        --element->hrepeat;
    else
        nexusrv_hist_array_pop(decoder->res_hists);
    return tnt;
}

static uint32_t nexusrv_trace_available_icnt(nexusrv_trace_decoder *decoder) {
    uint32_t icnt = decoder->res_icnt;
    if (!decoder->msg_present)
        goto done_msg;
    assert(!nexusrv_msg_is_res(&decoder->msg));
    if (nexusrv_msg_has_icnt(&decoder->msg))
        icnt += decoder->msg.icnt;
done_msg:
    assert(icnt >= decoder->consumed_icnt);
    return icnt - decoder->consumed_icnt;
}

static void nexusrv_trace_consume_icnt(nexusrv_trace_decoder *decoder, uint32_t icnt) {
    assert(nexusrv_trace_available_icnt(decoder) >= icnt);
    // At least res_icnt or consumed_icnt needs to be 0
    assert(!decoder->res_icnt || !decoder->consumed_icnt);
    if (decoder->res_icnt >= icnt) {
        decoder->res_icnt -= icnt;
        return;
    }
    assert(decoder->msg_present);
    decoder->consumed_icnt += icnt - decoder->res_icnt;
    decoder->res_icnt = 0;
}

static int nexusrv_trace_pull_msg(nexusrv_trace_decoder *decoder) {
    int rc = nexusrv_trace_fetch_msg(decoder);
    if (rc < 0)
        return rc;
    if (!nexusrv_msg_is_res(&decoder->msg))
        return rc;
    if (nexusrv_hist_array_size(decoder->res_hists) >= MSG_ICNT_MAX)
        return -nexus_trace_hist_overflow;
    nexusrv_hist_arr_element element = {};
    element.timestamp = decoder->msg.timestamp;
    if (nexusrv_msg_has_icnt(&decoder->msg)) {
        // Still need to append an empty element to track time
        decoder->res_icnt += decoder->msg.icnt;
        if (decoder->res_icnt > UINT32_MAX - MSG_ICNT_MAX)
            return -nexus_trace_icnt_overflow;
    } else if (nexusrv_msg_has_hist(&decoder->msg)) {
        element.hist = decoder->msg.hist;
        element.hrepeat = decoder->msg.hrepeat;
    } else if (decoder->msg_decoder->hw_cfg->ext_sifive) {
        // Sifive Extension
        switch (decoder->msg.res_code) {
            case 8:
                element.hist = 0b10;
                element.hrepeat = decoder->msg.res_data;
                break;
            case 9:
                element.hist = 0b11;
                element.hrepeat = decoder->msg.res_data;
                break;
            default:
                return -nexus_msg_unsupported;
        }
    } else
        return -nexus_msg_unsupported;
    rc = nexusrv_hist_array_push(decoder->res_hists, element);
    if (rc < 0)
        return rc;
    uint32_t tnts = (1 + element.hrepeat) *
                    nexusrv_msg_hist_bits(element.hist);
    decoder->res_tnts += tnts;
    // Consume the ResourceFull msg
    decoder->msg_present = 0;
    return 1;
}

static void nexusrv_trace_retire_msg(nexusrv_trace_decoder *decoder) {
    assert(decoder->msg_present);
    assert(decoder->msg.tcode != NEXUSRV_TCODE_ResourceFull);
    assert(!decoder->res_icnt);
    assert(!nexusrv_hist_array_size(decoder->res_hists));
    if (nexusrv_msg_has_icnt(&decoder->msg))
        assert(decoder->consumed_icnt == decoder->msg.icnt);
    else
        assert(!decoder->consumed_icnt);
    if (nexusrv_msg_has_hist(&decoder->msg))
        assert(decoder->consumed_tnts ==
                nexusrv_msg_hist_bits(decoder->msg.hist));
    else
        assert(!decoder->consumed_tnts);
    decoder->consumed_icnt = 0;
    decoder->consumed_tnts = 0;
    if (nexusrv_msg_is_branch(&decoder->msg)) {
        if (nexusrv_msg_is_sync(&decoder->msg)) {
            decoder->timestamp = decoder->msg.timestamp;
            // Downgrade to ProgTraceSync, but do not retire it yet
            decoder->msg.tcode = NEXUSRV_TCODE_ProgTraceSync;
            decoder->msg.icnt = 0;
            decoder->msg.hist = 0;
        } else {
            nexusrv_trace_retire_timestamp(decoder,
                                           &decoder->msg.timestamp,
                                           &decoder->msg.hrepeat);
            if (decoder->msg.hrepeat)
                --decoder->msg.hrepeat;
            else
                decoder->msg_present = 0;
        }
        return;
    }
    if (nexusrv_msg_is_sync(&decoder->msg)) {
        decoder->timestamp = decoder->msg.timestamp;
        decoder->full_addr = decoder->msg.xaddr;
        nexusrv_retstack_clear(&decoder->return_stack);
    } else
        nexusrv_trace_retire_timestamp(decoder,
                                       &decoder->msg.timestamp,
                                       &decoder->msg.hrepeat);
    decoder->msg_present = 0;
    return;
}

int nexusrv_trace_sync_reset(nexusrv_trace_decoder* decoder,
                             nexusrv_trace_sync *sync) {
    if (decoder->synced)
        return 0;
    for (;;) {
        int rc = nexusrv_trace_fetch_msg(decoder);
        if (rc < 0)
            return rc;
        if (nexusrv_msg_is_sync(&decoder->msg))
            break;
        decoder->msg_present = 0;
    }
    nexusrv_hist_array_clear(decoder->res_hists);
    decoder->res_tnts = 0;
    decoder->res_icnt = 0;
    decoder->consumed_tnts = 0;
    decoder->consumed_icnt = 0;
    decoder->synced = 1;
    // Downgrade to ProgTraceSync
    decoder->msg.tcode = NEXUSRV_TCODE_ProgTraceSync;
    return nexusrv_trace_next_sync(decoder, sync);
}

int32_t nexusrv_trace_try_retire(nexusrv_trace_decoder *decoder,
                                 uint32_t icnt, unsigned *event) {
    if (!decoder->synced)
        return -nexus_trace_not_synced;
    // Ensure the return value does not overflow
    if (icnt > INT32_MAX)
        icnt = INT32_MAX;
    *event = 0;
    int rc;
    do {
        // Consume ResourceFull Msgs until we have requested icnt,
        // Or stopped at other Msgs.
        uint32_t avail_icnt = nexusrv_trace_available_icnt(decoder);
        if (icnt < avail_icnt) {
            nexusrv_trace_consume_icnt(decoder, icnt);
            return icnt;
        }
        rc = nexusrv_trace_pull_msg(decoder);
        if (rc < 0)
            return rc;
        // Try again, as we have consumed something
    } while (rc);
    icnt = nexusrv_trace_available_icnt(decoder);
    nexusrv_trace_consume_icnt(decoder, icnt);
    if (nexusrv_trace_available_tnts(decoder)) {
        *event = NEXUSRV_Trace_Event_Direct;
        return icnt;
    }
    if (!decoder->msg_present) {
        return icnt;
    }
    // Check invariants
    if (nexusrv_msg_has_hist(&decoder->msg))
        assert(decoder->consumed_tnts ==
               nexusrv_msg_hist_bits(decoder->msg.hist));
    if (nexusrv_msg_is_branch(&decoder->msg))
        *event = nexusrv_msg_is_indir_branch(&decoder->msg) ?
                 (decoder->msg.branch_type ?
                    NEXUSRV_Trace_Event_Trap :
                    NEXUSRV_Trace_Event_Indirect) :
                 NEXUSRV_Trace_Event_Direct;
    else if (nexusrv_msg_is_sync(&decoder->msg))
        *event = NEXUSRV_Trace_Event_Sync;
    else switch (decoder->msg.tcode) {
        case NEXUSRV_TCODE_Error:
            *event = NEXUSRV_Trace_Event_Error;
            break;
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            *event = NEXUSRV_Trace_Event_Stop;
            break;
    }
    return icnt;
}

int nexusrv_trace_next_tnt(nexusrv_trace_decoder *decoder) {
    if (!decoder->synced)
        return -nexus_trace_not_synced;
    int rc;
    do {
        if (nexusrv_trace_available_tnts(decoder))
            return nexusrv_trace_consume_tnt(decoder);
        rc = nexusrv_trace_pull_msg(decoder);
        if (rc < 0)
            return rc;
        // Try again, as we have consumed something
    } while(rc);
    assert(decoder->msg_present);
    if (nexusrv_trace_available_icnt(decoder))
        return false;
    if (!nexusrv_msg_is_branch(&decoder->msg) ||
        nexusrv_msg_is_indir_branch(&decoder->msg))
        return -nexus_trace_mismatch;
    nexusrv_trace_retire_msg(decoder);
    return true;
}

void nexusrv_trace_push_call(nexusrv_trace_decoder* decoder,
                             uint64_t callsite) {
    nexusrv_retstack_push(&decoder->return_stack, callsite);
}

int nexusrv_trace_pop_ret(nexusrv_trace_decoder* decoder,
                          uint64_t *callsite) {
    if (!nexusrv_retstack_used(&decoder->return_stack))
        return -nexus_trace_retstack_empty;
    *callsite = nexusrv_retstack_pop(&decoder->return_stack);
    return 1;
}

unsigned nexusrv_trace_callstack_used(nexusrv_trace_decoder* decoder) {
    return nexusrv_retstack_used(&decoder->return_stack);
}

int nexusrv_trace_next_indirect(nexusrv_trace_decoder *decoder,
                                nexusrv_trace_indirect *indir) {
    if (!decoder->synced)
        return -nexus_trace_not_synced;
    int rc = nexusrv_trace_pull_msg(decoder);
    if (rc < 0)
        return rc;
    if (nexusrv_trace_available_icnt(decoder) ||
        nexusrv_trace_available_tnts(decoder))
        return -nexus_trace_mismatch;
    if (!nexusrv_msg_is_branch(&decoder->msg) ||
        !nexusrv_msg_is_indir_branch(&decoder->msg))
        return -nexus_trace_mismatch;
    if (nexusrv_msg_is_sync(&decoder->msg))
        decoder->full_addr = decoder->msg.xaddr;
    else {
        decoder->full_addr ^= decoder->msg.xaddr;
        decoder->msg.xaddr = 0;
    }
    indir->target = decoder->full_addr << 1;
    indir->exception = 0;
    indir->interrupt = 0;
    switch (decoder->msg.branch_type) {
        case 1:
            indir->interrupt = 1;
            __attribute__((fallthrough));
        case 2:
            indir->exception = 1;
            break;
        case 3:
            indir->interrupt = 1;
            break;
    }
    nexusrv_trace_retire_msg(decoder);
    indir->ownership = 0;
    // Try if the next msg is ownership
    nexusrv_msg msg2;
    rc = nexusrv_msg_decoder_next(decoder->msg_decoder, &msg2);
    if (rc < 0)
        return rc;
    if (msg2.tcode != NEXUSRV_TCODE_Ownership) {
        nexusrv_msg_decoder_rewind_last(decoder->msg_decoder);
        return 1;
    }
    indir->ownership = 1;
    indir->ownership_fmt = msg2.ownership_fmt;
    indir->ownership_priv = msg2.ownership_priv;
    indir->ownership_v = msg2.ownership_v;
    indir->context = msg2.context;
    return 1;
}

int nexusrv_trace_next_sync(nexusrv_trace_decoder *decoder, nexusrv_trace_sync *sync) {
    if (!decoder->synced)
        return -nexus_trace_not_synced;
    int rc = nexusrv_trace_pull_msg(decoder);
    if (rc < 0)
        return rc;
    if (nexusrv_trace_available_icnt(decoder) ||
        nexusrv_trace_available_tnts(decoder))
        return -nexus_trace_mismatch;
    if (!nexusrv_msg_is_sync(&decoder->msg) ||
        nexusrv_msg_is_branch(&decoder->msg))
        return -nexus_trace_mismatch;
    sync->sync = decoder->msg.sync_type;
    sync->addr = decoder->msg.xaddr << 1;
    nexusrv_trace_retire_msg(decoder);
    return 1;
}

int nexusrv_trace_next_error(nexusrv_trace_decoder *decoder, nexusrv_trace_error *error) {
    if (!decoder->synced)
        return -nexus_trace_not_synced;
    int rc = nexusrv_trace_pull_msg(decoder);
    if (rc < 0)
        return rc;
    if (nexusrv_trace_available_icnt(decoder) ||
        nexusrv_trace_available_tnts(decoder))
        return -nexus_trace_mismatch;
    if (decoder->msg.tcode != NEXUSRV_TCODE_Error)
        return -nexus_trace_mismatch;
    error->ecode = decoder->msg.error_code;
    error->etype = decoder->msg.error_type;
    nexusrv_trace_retire_msg(decoder);
    assert(!decoder->msg_present);
    decoder->synced = 0;
    return 1;
}

int nexusrv_trace_next_stop(nexusrv_trace_decoder *decoder, nexusrv_trace_stop *stop) {
    if (!decoder->synced)
        return -nexus_trace_not_synced;
    int rc = nexusrv_trace_pull_msg(decoder);
    if (rc < 0)
        return rc;
    if (nexusrv_trace_available_icnt(decoder) ||
        nexusrv_trace_available_tnts(decoder))
        return -nexus_trace_mismatch;
    if (decoder->msg.tcode != NEXUSRV_TCODE_ProgTraceCorrelation)
        return -nexus_trace_mismatch;
    stop->evcode = decoder->msg.stop_code;
    nexusrv_trace_retire_msg(decoder);
    assert(!decoder->msg_present);
    decoder->synced = 0;
    return 1;
}