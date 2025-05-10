// SPDX-License-Identifer: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_OPTS_DEF_H
#define LIBNEXUS_RV_OPTS_DEF_H

#define OPT_PARSE_BEGIN {                           \
    int optidx = 0;                                 \
    int optchar;                                    \
    while ((optchar = getopt_long(                  \
        argc, argv,                                 \
        short_opts, long_opts, &optidx)) != -1) {   \
        switch (optchar) {

#define OPT_PARSE_H_HELP                            \
        case 'h':                                   \
            help(argv[0]);                          \
            return 1;

#define OPT_PARSE_W_HWCFG                           \
        case 'w':                                   \
            hwcfg_str = optarg;                     \
            break;

#define OPT_PARSE_C_CPU                             \
        case 'c':                                   \
            cpu = atoi(optarg);                     \
            break;

#define OPT_PARSE_B_BUFSZ                           \
        case 'b':                                   \
            bufsz = strtoul(optarg, NULL, 0);       \
            if (bufsz < NEXUS_RV_MSG_MAX_BYTES)     \
                error(-1, 0, "Buffer size cannot be smaller than %d", \
                NEXUS_RV_MSG_MAX_BYTES);            \
            break;

#define OPT_PARSE_X_TEXT                            \
        case 'x':                                   \
            text = true;                            \
            break;

#define OPT_PARSE_P_PREFIX                          \
        case 'p':                                   \
            prefix = optarg;                        \
            break;

#define OPT_PARSE_U_UCORE                           \
        case 'u':                                   \
            vm->load_core(make_shared<linux_ucore>( \
                optarg, sysroot_dirs, dbg_dirs));   \
            break;

#define OPT_PARSE_R_SYSROOT                         \
        case 'r':                                   \
            sysroot_dirs = split_dirs(optarg);      \
            break;

#define OPT_PARSE_D_DEBUGDIR                        \
        case 'd':                                   \
            dbg_dirs = split_dirs(optarg);          \
            break;

#define OPT_PARSE_P_PROCFS                          \
        case 'p':                                   \
            procfs = optarg;                        \
            break;

#define OPT_PARSE_Y_SYSFS                           \
        case 'y':                                   \
            sysfs = optarg;                         \
            break;

#define OPT_PARSE_K_KCORE                           \
        case 'k':                                   \
            vm->load_core(make_shared<linux_kcore>( \
                procfs, sysfs,                      \
                sysroot_dirs, dbg_dirs));           \
            break;

#define OPT_PARSE_END                               \
        default:                                    \
            return 1;                               \
        }                                           \
    }                                               \
}

#endif
