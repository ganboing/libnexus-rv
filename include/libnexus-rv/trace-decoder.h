// SPDX-License-Identifier: Apache 2.0
/*
 * trace-decoder.h - NexusRV Trace decoder functions and declarations
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_TRACE_DECODER_H
#define LIBNEXUS_RV_TRACE_DECODER_H

#include "msg-decoder.h"
#include "hist-array.h"
#include "return-stack.h"

/**
 * @file
 * @brief Trace decoder usage model, events, and common error codes
 *
 * The main function is nexusrv_trace_try_retire. The user of the trace
 * decoder is expected to be calling this function a lot. Assuming we are
 * currently synchronized to a code block (a sequence of instructions without
 * branch in between). The caller can try to retire the whole block by
 * calling it, and see how many instructions can be successfully retired
 * before hitting a *event*. This allows the caller to determine if there's
 * any interrupt/exception occurred in the middle of the block, and if the
 * Direct/Indirect branch or other expected event is pending at the end of
 * the block. The events can then be consumed by calling the corresponding
 * nexusrv_trace_next_x function.
 * * \b NEXUSRV_Trace_Event_None:
 *     In BTM mode, no event pending. In HTM mode, a TNT might be pending.
 *     Use nexusrv_trace_next_tnt to retrieve it
 * * \b NEXUSRV_Trace_Event_Direct:
 *     A direct branch is pending.
 *     Use nexusrv_trace_next_tnt to retrieve it
 * * \b NEXUSRV_Trace_Event_DirectSync:
 *     A direct branch+sync is pending.
 *     Use nexusrv_trace_next_tnt to retrieve the direct branch, or
 *     Use nexusrv_trace_next_sync to retrieve the sync.
 *     Note: use nexusrv_trace_next_sync will discard the direct branch.
 * * \b NEXUSRV_Trace_Event_Indirect:
 *     An indirect branch is pending
 *     Use nexusrv_trace_next_indirect to retrieve it
 * * \b NEXUSRV_Trace_Event_IndirectSync:
 *     An indirect branch+sync is pending
 *     Use nexusrv_trace_next_indirect to retrieve the indirect branch, or
 *     Use nexusrv_trace_next_sync to retrieve the sync.
 *     Note: use nexusrv_trace_next_sync will discard the indirect branch.
 * * \b NEXUSRV_Trace_Event_Sync: Use nexusrv_trace_next_sync to retrieve it
 * * \b NEXUSRV_Trace_Event_Stop: Use nexusrv_trace_next_stop to retrieve it
 * * \b NEXUSRV_Trace_Event_Error: Use nexusrv_trace_next_error to retrieve it
 *
 * Error handling:
 * For all the hard errors listed below, decoding should be aborted,
 * and the only thing the caller can do is to free the trace decoder
 * by calling nexusrv_trace_decoder_fini.\n
 * Trace decoder returns error codes at different levels:
 * * \b -nexus_no_mem:
 *   Failed to allocate memory. This is a \b hard error.
 * * \b -nexus_stream_bad_mseo, \b -nexus_stream_truncate,
 *   \b -nexus_stream_read_failed:
 *   Failed to fetch Message from trace file. It's reported by the Message
 *   decoder. This is a \b hard error.
 * * \b -nexus_msg_invalid, \b -nexus_msg_missing_field:
 *   Failed to decoder the Message from trace file. It's reported by the
 *   Message decoder. This is a \b hard error.
 * * \b -nexus_msg_unsupported:
 *   The Message is unsupported by the trace decoder. The caller can resolve
 *   the situation by handling Message itself. (via nexusrv_msg_decoder_next)
 *   Once the Message is consumed, it can retry the trace decoder function.
 * * \b -nexus_trace_eof:
 *   Expecting to decode more Messages, but there's no Message left. Decoding
 *   should be terminated. This is expected when the trace has been terminated,
 *   with or without a stop event.
 * * \b -nexus_trace_not_synced:
 *   Trace decoder hasn't been synced. The trace decoder must be synced once
 *   before calling try_retire and next_x(). The next_stop() and next_error()
 *   will also desync the decoder if returned success, and decoder should be
 *   synced again.
 * * \b -nexus_trace_hist_overflow, \b -nexus_trace_icnt_overflow:
 *   There are too many HIST or I-CNT ResourceFull Messages being consumed,
 *   and exceeds the capability of the trace decoder. This is a \b hard error
 * * \b nexus_trace_mismatch:
 *   This happens when the caller tries to get certain event (branch,sync...),
 *   but there's no such event pending. It typically means the caller is
 *   misusing the trace decoder.
 */

/** @brief NexusRV Trace Indirect Branch Event
 *
 * If neither interrupt or exception is set, is a synchronous branch
 * If both are set, it can be either but NexusRV Message doesn't report
 * the exact type due to HW limitation
 */
typedef struct nexusrv_trace_indirect {
    uint64_t target;   /*!< Target Address */
    uint64_t context;
    /*!< PROCESS.CONTEXT from OWNERSHIP Message, valid if ownership=1 */
    struct {
        uint8_t interrupt : 1; /*!< Is Interrupt */
        uint8_t exception : 1; /*!< Is Exception */
        uint8_t ownership : 1;      /*!< OWNERSHIP Message present */
        uint8_t ownership_fmt : 2;  /*!< PROCESS.FORMAT from OWNERSHIP */
        uint8_t ownership_priv : 2; /*!< PROCESS.PRV from OWNERSHIP */
        uint8_t ownership_v : 1;    /*!< PROCESS.V from OWNERSHIP */
    };
} nexusrv_trace_indirect;

/** @brief NexusRV Trace Sync Event
 */
typedef struct nexusrv_trace_sync {
    uint64_t addr;        /*!< F-ADDR from NexusRV Message */
    struct {
        uint8_t sync : 4; /*!< B-TYPE from NexusRV Message */
    };
} nexusrv_trace_sync;

/** @brief NexusRV Trace Error Event
 */
typedef struct nexusrv_trace_error {
    uint32_t ecode;        /*!< ECODE from NexusRV Message */
    struct {
        uint8_t etype : 4; /*!< ETYPE from NexusRV Message */
    };
} nexusrv_trace_error;

/** @brief NexusRV Stop Event
 */
typedef struct nexusrv_trace_stop {
    uint8_t evcode : 4;   /*!< EVCODE from NexusRV Message */
} nexusrv_trace_stop;

/** @brief NexusRV trace events
 */
enum nexusrv_trace_events {
    NEXUSRV_Trace_Event_None,
    NEXUSRV_Trace_Event_Direct,
    NEXUSRV_Trace_Event_DirectSync,
    NEXUSRV_Trace_Event_Trap,
    NEXUSRV_Trace_Event_Indirect,
    NEXUSRV_Trace_Event_IndirectSync,
    NEXUSRV_Trace_Event_Sync,
    NEXUSRV_Trace_Event_Stop,
    NEXUSRV_Trace_Event_Error,
};

static inline const char
*str_nexusrv_trace_event(unsigned event) {
    switch (event) {
        case NEXUSRV_Trace_Event_None:
            return "none";
        case NEXUSRV_Trace_Event_Direct:
            return "direct";
        case NEXUSRV_Trace_Event_DirectSync:
            return "direct-sync";
        case NEXUSRV_Trace_Event_Trap:
            return "trap";
        case NEXUSRV_Trace_Event_Indirect:
            return "indirect";
        case NEXUSRV_Trace_Event_IndirectSync:
            return "indirect-sync";
        case NEXUSRV_Trace_Event_Sync:
            return "sync";
        case NEXUSRV_Trace_Event_Stop:
            return "stop";
        case NEXUSRV_Trace_Event_Error:
            return "error";
        default:
            return "unknown";
    }
}


/** @brief NexusRV Trace decoder context
 *
 * This should be initialized by nexusrv_trace_decoder_init
 * before calling trace decoder functions, and finalized by
 * nexusrv_trace_decoder_fini to release resources.
 */
typedef struct nexusrv_trace_decoder {
    nexusrv_msg_decoder *msg_decoder;
    /*!< Pointer to Message decoder context */
    struct nexusrv_hist_array* res_hists;
    /*!< Accumulated HISTs in ResourceFull Messages */
    uint32_t res_icnt;      /*!< Accumulated I-CNT in ResourceFull Messages */
    uint32_t res_tnts;      /*!< The sum of TNTs in res_hists */
    uint32_t consumed_icnt; /*!< Consumed I-CNT so far */
    uint8_t consumed_tnts;  /*!< Consumed TNTs so far */
    bool synced;            /*!< Has been synced by SYNC Message? */
    bool msg_present;       /*!< Indicator whether buffered Message is valid */
    nexusrv_msg msg;        /*!< The buffered Message */
    uint64_t full_addr;     /*!< Address tracking */
    uint64_t timestamp;     /*!< Timestamp tracking */
    nexusrv_return_stack return_stack; /*!< Return stack tracking */
} nexusrv_trace_decoder;

/** @brief Initialize the trace decoder
 *
 * The trace decoder will start using the Message decoder \p msg_decoder
 * It'll not keep state of the Message decoder, so the Message decoder
 * can still be invoked out of trace decoder context to handle non-standard
 * Messages or custom Messages the trace decoder can't handle.
 *
 * @param [out] decoder The decoder context
 * @param [in] msg_decoder The Message decoder context
 * @retval ==0: Success
 * @retval -nexus_no_mem: Out of memory
 */
int nexusrv_trace_decoder_init(nexusrv_trace_decoder* decoder,
                               nexusrv_msg_decoder *msg_decoder);

/** @brief Finialize the trace decoder
 *
 * Memory allocated by the trace decoder will be free'ed
 *
 * @param [in] decoder The decoder context
 */
void nexusrv_trace_decoder_fini(nexusrv_trace_decoder* decoder);

/** @brief Get current timestamp
 *
 * Returns the current time tracked by the decoder. The time is the
 * timestamp the last Message was retired. For Messages with hrepeat,
 * effectively it'll be retired (hrepeat + 1) times, and the delta
 * time is divided by (hrepeat + 1).
 *
 * @param [in] decoder The decoder context
 * @return The timestamp
 */
static inline uint64_t nexusrv_trace_time(nexusrv_trace_decoder* decoder) {
    const nexusrv_hw_cfg *hwcfg = decoder->msg_decoder->hw_cfg;
    uint64_t time = decoder->timestamp;
    if (hwcfg->ts_bits < 64)
        time &= (((uint64_t)1 << hwcfg->ts_bits) - 1);
    if (hwcfg->timer_freq) {
        unsigned __int128 normalized = time;
        normalized *= 1000UL * 1000 * 1000;
        time = normalized / hwcfg->timer_freq;
    }
    return time;
}

/** @brief Get the available I-CNT before needing to fetch new messages
 *
 * @param [in] decoder The decoder context
 * @return I-CNT
 */
uint32_t nexusrv_trace_available_icnt(nexusrv_trace_decoder *decoder);

/** @brief Try to retire \p icnt from trace.
 *
 * This is the main function the trace encoder provides. Usually it's
 * intended to be used by the caller who has the knowledge of the context
 * and the instructions to execute. The caller can try to retire \p icnt
 * that represents the block of sequential instructions (without branch)
 * and see if successful. The decoder will report the maximum I-CNT it
 * can retire before it hits any event, and report the first in \p event.
 * If there's no event less than the range of \p icnt, no event will be
 * reported, and \p icnt will be returned. To avoid overflow, \p icnt
 * will be capped at INT32_MAX \n
 *
 * For the caller that has no knowledge of context or instructions, it
 * can try to retire INT32_MAX I-CNT, and check the actual I-CNT retired.
 * \n Breakdown of events reported: \n
 *  NEXUSRV_Trace_Event_None:
 *    None or potentially a TNT (in HTM mode). \n
 *  NEXUSRV_Trace_Event_Direct:
 *    Direct branch, use nexusrv_trace_next_tnt to retrieve \n
 *  NEXUSRV_Trace_Event_Trap, NEXUSRV_Trace_Event_Indirect:
 *    Indirect branch, use nexusrv_trace_next_indirect to retrieve \n
 *  NEXUSRV_Trace_Event_Sync:
 *    Sync event, use nexusrv_trace_next_sync to retrieve \n
 *  NEXUSRV_Trace_Event_Stop:
 *    Stop event, use nexusrv_trace_next_stop to retrieve \n
 *  NEXUSRV_Trace_Event_Error:
 *    Error event, use nexusrv_trace_next_error to retrieve \n
 *
 * @param [in] decoder The decoder context
 * @param icnt I-CNT intended to retire
 * @param [out] event Encountered event to report
 * @retval >=0: No error occurred, and the actual I-CNT retired
 * @retval <0: Error occurred, refer to common errors section
 */
int32_t nexusrv_trace_try_retire(nexusrv_trace_decoder *decoder,
                                 uint32_t icnt,
                                 unsigned *event);

/** @brief Synchronize the trace decoder
 *
 * If the decoder is already synchronized, do nothing. Else, forward till we
 * get the next SYNC Message, and synchronize the trace decoder using it.
 * The I-CNT and HIST will be cleared. The Address and Timestamp will be set
 * to the Message. To not lose track of the reason of the SYNC Message, the
 * caller should store the \p sync event when there is one.
 *
 * @param [in] decoder The decoder context
 * @param [out] sync The Sync event
 * @retval ==0: Trace decoder is already synchronized
 * @retval >0: Trace decoder is unsynced -> synced. The \p sync is set to
 *   the synchronization event indicating the address and reason.
 * @retval <0: Error occurred, refer to common errors section
 */
int nexusrv_trace_sync_reset(nexusrv_trace_decoder* decoder,
                             nexusrv_trace_sync *sync);

/** @brief Get the next taken-not-taken
 *
 * @param [in] decoder The decoder context
 * @retval ==0: Branch not taken
 * @retval >0: Branch taken
 * @retval <0: Error occurred, refer to common errors section
 */
int nexusrv_trace_next_tnt(nexusrv_trace_decoder *decoder);

int nexusrv_trace_push_call(nexusrv_trace_decoder* decoder,
                            uint64_t callsite);

int nexusrv_trace_pop_ret(nexusrv_trace_decoder* decoder,
                          uint64_t *callsite);

unsigned nexusrv_trace_callstack_used(nexusrv_trace_decoder* decoder);

/** @brief Get the next indirect branch
 *
 * @param [in] decoder The decoder context
 * @param [out] indirect The consumed indirect branch
 * @retval >0: Successfully got Indirect branch
 * @retval <0: Error occurred, refer to common errors section
 */
int nexusrv_trace_next_indirect(nexusrv_trace_decoder *decoder,
                                nexusrv_trace_indirect *indirect);

/** @brief Get the next Sync event
 *
 * @param [in] decoder The decoder context
 * @param [out] sync The consumed Sync Event
 * @retval >0: Successfully got Sync event
 * @retval <0: Error occurred, refer to common errors section
 */
int nexusrv_trace_next_sync(nexusrv_trace_decoder *decoder,
                            nexusrv_trace_sync *sync);

/** @brief Get the next Error event
 *
 * @param [in] decoder The decoder context
 * @param [out] error The consumed Error Event
 * @retval >0: Successfully got Error event
 * @retval <0: Error occurred, refer to common errors section
 */
int nexusrv_trace_next_error(nexusrv_trace_decoder *decoder,
                             nexusrv_trace_error *error);

/** @brief Get the next Stop event
 *
 * @param [in] decoder The decoder context
 * @param [out] stop The consumed Stop Event
 * @retval >0: Successfully got Stop event
 * @retval <0: Error occurred, refer to common errors section
 */
int nexusrv_trace_next_stop(nexusrv_trace_decoder *decoder,
                            nexusrv_trace_stop *stop);

/** @brief Add(retire) the timestamp to decoder
 *
 * For scenarios where the trace decoder doesn't recognize the message,
 * and delegate it to the caller, the caller should use this function
 * to add the timestamp back to the trace decoder, if any.
 *
 * @param [in] decoder decoder The decoder context
 * @param [in] timestamp The timestamp from the Message
 */
void nexusrv_trace_add_timestamp(nexusrv_trace_decoder *decoder,
                                 uint64_t timestamp);

#endif
