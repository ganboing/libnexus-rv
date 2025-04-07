// SPDX-License-Identifer: Apache 2.0
/*
 * print.c - Nexus-RV message printer
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdio.h>
#include <inttypes.h>
#include "print.h"

void printMsg(FILE *Fp, NexusRvMsg *Msg) {
    fprintf(Fp, "Src=%02" PRIu32 " Time=0x%06" PRIx64 " %s ",
            (uint32_t)Msg->Common.Source,
            (uint64_t)Msg->Common.TimeStamp,
            NexusRvTcodeStr(Msg->Common.TCode));
    switch (Msg->Common.TCode) {
        case NexusRvTcodeOwnership:
            fprintf(Fp, "Format=%u PRV=%u V=%u Context=0x%" PRIx64 "\n",
                    Msg->Ownership.Format,
                    Msg->Ownership.PRV,
                    Msg->Ownership.V,
                    Msg->Ownership.Context);
            break;
        case NexusRvTcodeError:
            fprintf(Fp, "ETYPE=%u ECODE=0x%" PRIx32 "\n",
                    Msg->Error.EType,
                    Msg->Error.Ecode);
            break;
        case NexusRvTcodeDirectBranch:
            fprintf(Fp, "ICNT=%" PRIu32,
                    Msg->DirectBranch.ICnt);
            break;
        case NexusRvTcodeIndirectBranch:
            fprintf(Fp, "ICNT=%" PRIu32 " BTYPE=%u UADDR=0x%" PRIx64 "\n",
                    Msg->IndirectBranch.ICnt,
                    Msg->IndirectBranch.BType,
                    (uint64_t)Msg->IndirectBranch.XAddr);
            break;
        case NexusRvTcodeProgTraceSync:
            fprintf(Fp, "ICNT=%" PRIu32 " SYNC=%u FADDR=0x%" PRIx64 "\n",
                    Msg->ProgTraceSync.ICnt,
                    Msg->ProgTraceSync.Sync,
                    (uint64_t)Msg->ProgTraceSync.XAddr);
            break;
        case NexusRvTcodeDirectBranchSync:
            fprintf(Fp, "ICNT=%" PRIu32 " SYNC=%u FADDR=0x%" PRIx64 "\n",
                    Msg->DirectBranchSync.ICnt,
                    Msg->DirectBranchSync.Sync,
                    (uint64_t)Msg->DirectBranchSync.XAddr);
            break;
        case NexusRvTcodeIndirectBranchSync:
            fprintf(Fp, "ICNT=%" PRIu32 " SYNC=%u BTYPE=%u FADDR=0x%" PRIx64 "\n",
                    Msg->IndirectBranchSync.ICnt,
                    Msg->IndirectBranchSync.Sync,
                    Msg->IndirectBranchSync.BType,
                    (uint64_t)Msg->IndirectBranchSync.XAddr);
            break;
        case NexusRvTcodeIndirectBranchHist:
            fprintf(Fp, "ICNT=%" PRIu32 " BTYPE=%u UADDR=0x%" PRIx64 " HIST=0x%" PRIx32 "\n",
                    Msg->IndirectBranchHist.ICnt,
                    Msg->IndirectBranchHist.BType,
                    (uint64_t)Msg->IndirectBranchHist.XAddr,
                    Msg->IndirectBranchHist.Hist);
            break;
        case NexusRvTcodeIndirectBranchHistSync:
            fprintf(Fp, "ICNT=%" PRIu32 " SYNC=%u BTYPE=%u FADDR=0x%" PRIx64 " HIST=0x%" PRIx32 "\n",
                    Msg->IndirectBranchHistSync.ICnt,
                    Msg->IndirectBranchHistSync.Sync,
                    Msg->IndirectBranchHistSync.BType,
                    (uint64_t)Msg->IndirectBranchHistSync.XAddr,
                    Msg->IndirectBranchHistSync.Hist);
            break;
        case NexusRvTcodeResourceFull:
            fprintf(Fp, "RCODE=%u",
                    Msg->ResourceFull.RCode);
            switch (Msg->ResourceFull.RCode) {
                case 0:
                    fprintf(Fp, " ICNT=%" PRIu32 "\n",
                            Msg->ResourceFull.ICnt);
                    break;
                case 1:
                    fprintf(Fp, " HIST=0x%" PRIx32 "\n",
                            Msg->ResourceFull.Hist);
                    break;
                case 2:
                    fprintf(Fp, " HIST=0x%" PRIx32 " HREPEAT=%" PRIu32 "\n",
                            Msg->ResourceFull.Hist,
                            Msg->ResourceFull.HRepeat);
                    break;
                default:
                    fprintf(Fp, " RDATA=0x%" PRIx32 "\n",
                            Msg->ResourceFull.RDATA);
                    break;
            }
            break;
        case NexusRvTcodeRepeatBranch:
            fprintf(Fp, "BCNT=%" PRIu32 "\n",
                    Msg->RepeatBranch.HRepeat);
            break;
        case NexusRvTcodeProgTraceCorrelation:
            fprintf(Fp, "EVCODE=%u ICNT=%" PRIu32,
                    Msg->ProgTraceCorrelation.EVCode,
                    Msg->ProgTraceCorrelation.ICnt);
            if (Msg->ProgTraceCorrelation.Hist)
                fprintf(Fp," HIST=0x%" PRIx32 "\n",
                        Msg->ProgTraceCorrelation.Hist);
            else
                fputc('\n', Fp);
            break;
        default:
            fprintf(Fp, "TCODE=%u\n",
                    Msg->Common.TCode);
            break;
    }
}