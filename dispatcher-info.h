#ifndef _INSTREW_RERUNNER_DISPATCHER_INFO_H
#define _INSTREW_RERUNNER_DISPATCHER_INFO_H

#include "common.h"

struct DispatcherInfo
{
    void (*loop_func)(uint64_t *cpu_regs);
    uintptr_t quick_dispatch_func;
    uintptr_t full_dispatch_func;

    uint8_t patch_data_reg;
};

#endif