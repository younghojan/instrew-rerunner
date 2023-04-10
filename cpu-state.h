#ifndef _INSTREW_RERUNNER_CPU_STATE_H
#define _INSTREW_RERUNNER_CPU_STATE_H

#include "common.h"
#include "rtld.h"

#include <asm/siginfo.h>
#include <asm/signal.h>

#define QUICK_TLB_BITS 10

struct State
{
    Rtld rtld;
    struct sigaction sigact[_NSIG];
    uint64_t rew_time;
};

struct CpuState
{
    struct CpuState *self;
    struct State *state;

    uintptr_t _unused[6];

    _Alignas(64) uint8_t regdata[0x400];

    _Alignas(64) uint64_t quick_tlb[1 << QUICK_TLB_BITS][2];

    _Atomic volatile int sigpending;
    sigset_t sigmask;
    stack_t sigaltstack;
    struct siginfo siginfo;
};

#define CPU_STATE_REGDATA_OFFSET 0x40
_Static_assert(offsetof(struct CpuState, regdata) == CPU_STATE_REGDATA_OFFSET,
               "CPU_STATE_REGDATA_OFFSET mismatch");

#define CPU_STATE_QTLB_OFFSET 0x440
_Static_assert(offsetof(struct CpuState, quick_tlb) == CPU_STATE_QTLB_OFFSET,
               "CPU_STATE_QTLB_OFFSET mismatch");

#define CPU_STATE_FROM_REGS(regdata) ((struct CpuState *)((char *)regdata - CPU_STATE_REGDATA_OFFSET))

#endif