#ifndef _INSTREW_RUNNER_EMULATE_H
#define _INSTREW_RUNNER_EMULATE_H

#include "common.h"

struct State;

void signal_init(struct State *state);
void emulate_syscall(uint64_t *cpu_state);

#endif