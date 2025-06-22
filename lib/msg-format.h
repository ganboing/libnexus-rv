// SPDX-License-Identifer: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_MSG_FORMAT_H
#define LIBNEXUS_RV_MSG_FORMAT_H

#include <inttypes.h>

#define NEXUS_FMT_SRC           " Src=%u"
#define NEXUS_FMT_TIMESTAMP     " Time=%" PRIu64
#define NEXUS_FMT_TCODE         " TCODE=%u"
#define NEXUS_FMT_OWNER_FORMAT  " FORMAT=%u"
#define NEXUS_FMT_OWNER_PRV     " PRV=%u"
#define NEXUS_FMT_OWNER_V       " V=%u"
#define NEXUS_FMT_OWNER_CONTEXT " CONTEXT=0x%" PRIx64
#define NEXUS_FMT_ERROR_ETYPE   " ETYPE=%u"
#define NEXUS_FMT_ERROR_ECODE   " ECODE=0x%" PRIx32
#define NEXUS_FMT_DA_IDTAG      " IDTAG=%u"
#define NEXUS_FMT_DA_DQDATA     " DQDATA=0x%" PRIx64
#define NEXUS_FMT_ICNT          " ICNT=%" PRIu32
#define NEXUS_FMT_BCNT          " BCNT=%" PRIu32
#define NEXUS_FMT_SYNC          " SYNC=%u"
#define NEXUS_FMT_BTYPE         " BTYPE=%u"
#define NEXUS_FMT_XADDR         " XADDR=0x%" PRIx64
#define NEXUS_FMT_HIST          " HIST=0x%" PRIx32
#define NEXUS_FMT_RCODE         " RCODE=%u"
#define NEXUS_FMT_RDATA         " RDATA=0x%" PRIx32
#define NEXUS_FMT_HREPEAT       " HREPEAT=%" PRIu32
#define NEXUS_FMT_EVCODE        " EVCODE=%u"
#define NEXUS_FMT_CDF           " CDF=%u"

#endif
