// SPDX-License-Identifer: Apache 2.0
/*
 * decoder.h - Nexus-RV decoder functions and declarations
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_DECODER_H
#define LIBNEXUS_RV_DECODER_H

#include "error.h"
#include "msg-types.h"
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct NexusRvDecoderCfg {
    bool HasTimeStamp;
    int SrcBits;
} NexusRvDecoderCfg;

#define NEXUS_RV_MSG_MAX_BYTES 38

ssize_t nexusrv_sync_forward(const uint8_t *Buffer, size_t Limit);

ssize_t nexusrv_sync_backward(const uint8_t *Buffer, size_t Pos);

ssize_t nexusrv_decode(const NexusRvDecoderCfg *cfg,
                 const uint8_t *Buffer, size_t Limit, NexusRvMsg *Msg);

#endif
