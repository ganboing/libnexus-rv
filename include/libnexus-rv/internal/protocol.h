// SPDX-License-Identifier: Apache 2.0
/*
 * protocol.h - MESO/MDO protocol definition
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_INTERNAL_PROTOCOL_H
#define LIBNEXUS_RV_INTERNAL_PROTOCOL_H

#include <stdint.h>

#define NEXUS_RV_MESO_BITS 2
#define NEXUS_RV_MDO_BITS 6

typedef union nexusrv_msg_byte {
    uint8_t byte;
    struct {
        uint8_t mseo: NEXUS_RV_MESO_BITS;
        uint8_t mdo: NEXUS_RV_MDO_BITS;
    };
} nexusrv_msg_byte;

#endif
