#ifndef LIBNEXUS_RV_INST_HELPER_H
#define LIBNEXUS_RV_INST_HELPER_H

enum nexusrv_itypes {
    NEXUSRV_ITYPE_None,
    NEXUSRV_ITYPE_Exception,
    NEXUSRV_ITYPE_Cond_Branch,
    NEXUSRV_ITYPE_Direct_Jump,
    NEXUSRV_ITYPE_Direct_Call,
    NEXUSRV_ITYPE_Indirect_Jump,
    NEXUSRV_ITYPE_Indirect_Call,
    NEXUSRV_ITYPE_Coroutine_Swap,
    NEXUSRV_ITYPE_Function_Return,
};

static inline bool nexurv_inst_link_reg(unsigned reg) {
    return reg == 1 || reg == 5;
}

static inline enum nexusrv_itypes
        nexusrv_inst_jal_types(unsigned rd) {
    return nexurv_inst_link_reg(rd) ?
        NEXUSRV_ITYPE_Direct_Call :
        NEXUSRV_ITYPE_Direct_Jump;
}

static inline enum nexusrv_itypes
        nexusrv_inst_jalr_types(unsigned rd, unsigned rs) {
    if (nexurv_inst_link_reg(rd)) {
        if (!nexurv_inst_link_reg(rs))
            return NEXUSRV_ITYPE_Indirect_Call;
        return rd == rs ?
            NEXUSRV_ITYPE_Indirect_Call :
            NEXUSRV_ITYPE_Coroutine_Swap;
    }
    return nexurv_inst_link_reg(rs) ?
            NEXUSRV_ITYPE_Function_Return :
            NEXUSRV_ITYPE_Indirect_Jump;
}

static inline enum nexusrv_itypes
        nexusrv_inst_cjalr_types(unsigned rs) {
    return rs == 5 ?
            NEXUSRV_ITYPE_Coroutine_Swap :
            NEXUSRV_ITYPE_Indirect_Call;
}

static inline enum nexusrv_itypes
        nexusrv_inst_cjr_types(unsigned rs) {
    return nexurv_inst_link_reg(rs) ?
            NEXUSRV_ITYPE_Function_Return :
            NEXUSRV_ITYPE_Indirect_Jump;
}

#endif
