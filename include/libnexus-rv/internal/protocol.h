// SPDX-License-Identifer: Apache 2.0
/*
 * protocol.h - MESO/MDO protocol definition
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_PROTOCOL_H
#define LIBNEXUS_RV_PROTOCOL_H

#include <stdint.h>

#define NEXUS_RV_MESO_BITS 2
#define NEXUS_RV_MDO_BITS 6

typedef union NexusRvMsgByte {
    uint8_t Byte;
    struct {
        uint8_t MSEO: NEXUS_RV_MESO_BITS;
        uint8_t MDO: NEXUS_RV_MDO_BITS;
    };
} NexusRvMsgByte;

#endif
