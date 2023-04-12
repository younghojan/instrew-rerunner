#include <elf.h>
#include <sys/stat.h>

#include "common.h"
#include "cpu-state.h"
#include "dispatch.h"
#include "dispatcher-info.h"
#include "memory.h"
#include "rtld.h"

// Prototype to make compilers happy. This is used in the assembly HHVM
// dispatcher on x86-64 below.
uintptr_t resolve_func(struct CpuState *, uintptr_t, struct RtldPatchData *);

#define QUICK_TLB_BITS 10
#define QUICK_TLB_BITOFF 4 // must be either 1, 2, 3, or 4
// Clang's inline assembly doesn't support expressions for index scale.
// #define QUICK_TLB_IDXSCALE (1 << (4-QUICK_TLB_BITOFF))
#if QUICK_TLB_BITOFF == 4
#define QUICK_TLB_IDXSCALE 1
#elif QUICK_TLB_BITOFF == 3
#define QUICK_TLB_IDXSCALE 2
#elif QUICK_TLB_BITOFF == 2
#define QUICK_TLB_IDXSCALE 4
#elif QUICK_TLB_BITOFF == 1
#define QUICK_TLB_IDXSCALE 8
#else
#error "invalid QUICK_TLB_BITOFF"
#endif
#define QUICK_TLB_HASH(addr) (((addr) >> QUICK_TLB_BITOFF) & ((1 << QUICK_TLB_BITS) - 1))

#define PATH_MAX 256

GNU_FORCE_EXTERN
uintptr_t
resolve_func(struct CpuState *cpu_state, uintptr_t addr,
             struct RtldPatchData *patch_data)
{   
    struct State *state = cpu_state->state;

    if (patch_data)
        addr = patch_data->sym_addr;

    void *func;
    int retval = rtld_resolve(&state->rtld, addr, &func);
    if (UNLIKELY(retval < 0))
    {
        struct timespec start_time;
        struct timespec end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        void *obj_base;
        size_t obj_size;
        
        char file_path[PATH_MAX] = "", file_name[PATH_MAX] = "";
        strncpy(file_path, dir_path, sizeof(file_path));
        snprintf(file_name, sizeof(file_name), "func_%lx.elf", (unsigned long) addr);
        strncpy(file_path + strlen(file_path), file_name, sizeof(file_path));

        int fd = open(file_path, O_RDWR, 0);
        struct stat st;
        stat(file_path, &st);
        
        obj_size = ALIGN_UP(st.st_size, getpagesize());
        obj_base = mem_alloc_data(obj_size, getpagesize());
        read_full(fd, obj_base, obj_size);
        retval = rtld_add_object(&state->rtld, obj_base, obj_size, addr);

        if (retval < 0)
            goto error;
        retval = rtld_resolve(&state->rtld, addr, &func);
        if (retval < 0)
            goto error;

        clock_gettime(CLOCK_MONOTONIC, &end_time);
        size_t time_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
        state->rew_time += time_ns;
    }

    // If possible, patch code which caused us to get here.
    rtld_patch(patch_data, func);

    // Update quick TLB
    uintptr_t hash = QUICK_TLB_HASH(addr);
    cpu_state->quick_tlb[hash][0] = addr;
    cpu_state->quick_tlb[hash][1] = (uintptr_t)func;

    return (uintptr_t)func;

error:
    dprintf(2, "error resolving address %lx: %u\n", addr, -retval);
    _exit(retval);
}

// Used for PLT.
void dispatch_cdecl(uint64_t *);

inline void dispatch_cdecl(uint64_t *cpu_regs)
{
    struct CpuState *cpu_state = CPU_STATE_FROM_REGS(cpu_regs);
    uintptr_t addr = cpu_regs[0];
    uintptr_t hash = QUICK_TLB_HASH(addr);

    uintptr_t func = cpu_state->quick_tlb[hash][1];
    if (UNLIKELY(cpu_state->quick_tlb[hash][0] != addr))
        func = resolve_func(cpu_state, addr, NULL);

    void (*func_p)(void *);
    *((void **)&func_p) = (void *)func;
    func_p(cpu_regs);
}

static void
dispatch_cdecl_loop(uint64_t *cpu_regs)
{
    while (true)
    {
        dispatch_cdecl(cpu_regs);
    }
}

#ifdef __x86_64__

__attribute__((noreturn)) extern void dispatch_hhvm(uint64_t *cpu_state);
void dispatch_hhvm_tail();
void dispatch_hhvm_fullresolve();

#define QUICK_TLB_OFFSET_ASM(dest_reg, addr_reg) \
    lea dest_reg, [addr_reg * 4];                \
    and dest_reg, ((1 << QUICK_TLB_BITS) - 1) << (2 + QUICK_TLB_BITOFF);

ASM_BLOCK(
        .intel_syntax noprefix;

        // Stores result in r14, preserves all other registers
        .align 16;
        .type dispatch_hhvm_fullresolve, @function;
        dispatch_hhvm_fullresolve
        : // stack alignment: cdecl
          // Save all cdecl caller-saved registers.
        push rax;
        push rcx;
        push rdx;
        push rsi;
        push rdi;
        push r8;
        push r9;
        push r10;
        push r11;
        mov rdi, [r12 - CPU_STATE_REGDATA_OFFSET]; // cpu_state
        mov rsi, rbx;                              // addr
        mov rdx, r14;                              // patch data
        call resolve_func;
        mov r14, rax; // return value
        // Restore callee-saved registers.
        pop r11;
        pop r10;
        pop r9;
        pop r8;
        pop rdi;
        pop rsi;
        pop rdx;
        pop rcx;
        pop rax;
        jmp r14;
        .size dispatch_hhvm_fullresolve, .- dispatch_hhvm_fullresolve;

        .align 16;
        .global dispatch_hhvm_tail;
        .type dispatch_hhvm_tail, @function;
        dispatch_hhvm_tail
        : // stack alignment: cdecl
        mov r14, rbx;
        and r14, ((1 << QUICK_TLB_BITS) - 1) << QUICK_TLB_BITOFF;
        cmp rbx, [r12 + QUICK_TLB_IDXSCALE * r14 - CPU_STATE_REGDATA_OFFSET + CPU_STATE_QTLB_OFFSET];
        jne 1f;
        jmp [r12 + QUICK_TLB_IDXSCALE * r14 - CPU_STATE_REGDATA_OFFSET + CPU_STATE_QTLB_OFFSET + 8];
        .align 16;
        1
        : xor r14, r14; // zero patch data
        jmp dispatch_hhvm_fullresolve;
        .size dispatch_hhvm_tail, .- dispatch_hhvm_tail;

        .align 16;
        .global dispatch_hhvm_call;
        .type dispatch_hhvm_call, @function;
        dispatch_hhvm_call
        : // stack alignment: hhvm
        mov r14, rbx;
        and r14, ((1 << QUICK_TLB_BITS) - 1) << QUICK_TLB_BITOFF;
        cmp rbx, [r12 + QUICK_TLB_IDXSCALE * r14 - CPU_STATE_REGDATA_OFFSET + CPU_STATE_QTLB_OFFSET];
        jne 1f;
        call [r12 + QUICK_TLB_IDXSCALE * r14 - CPU_STATE_REGDATA_OFFSET + CPU_STATE_QTLB_OFFSET + 8];
        ret;
        .align 16;
        1
        : xor r14, r14; // zero patch data
        call dispatch_hhvm_fullresolve;
        ret;
        .size dispatch_hhvm_call, .- dispatch_hhvm_call;

        .align 16;
        .global dispatch_hhvm;
        .type dispatch_hhvm, @function;
        dispatch_hhvm
        : mov r12, rdi; // cpu_regs
        // Load HHVM registers
        mov rbx, [r12 + 0 * 8];
        mov rax, [r12 + 1 * 8];
        mov rcx, [r12 + 2 * 8];
        mov rdx, [r12 + 3 * 8];
        mov rbp, [r12 + 4 * 8];
        mov r15, [r12 + 5 * 8];
        mov r13, [r12 + 6 * 8];
        mov rsi, [r12 + 7 * 8];
        mov rdi, [r12 + 8 * 8];
        mov r8, [r12 + 9 * 8];
        mov r9, [r12 + 10 * 8];
        mov r10, [r12 + 11 * 8];
        mov r11, [r12 + 12 * 8];

        jmp 4f;

        .align 16;
        // This is the quick_tlb hot loop.
        2
        : call [r12 + QUICK_TLB_IDXSCALE * r14 - CPU_STATE_REGDATA_OFFSET + CPU_STATE_QTLB_OFFSET + 8];
        3
        : mov r14, rbx;
        and r14, ((1 << QUICK_TLB_BITS) - 1) << QUICK_TLB_BITOFF;
        cmp rbx, [r12 + QUICK_TLB_IDXSCALE * r14 - CPU_STATE_REGDATA_OFFSET + CPU_STATE_QTLB_OFFSET];
        je 2b;

        // This code isn't exactly cold, but should be executed not that often.
        // If we don't have addr in the quick_tlb, do a full resolve.
        4
        : xor r14, r14; // zero patch data
        call dispatch_hhvm_fullresolve;
        jmp 3b;
        .size dispatch_hhvm, .- dispatch_hhvm;

        .att_syntax;);

#endif // defined(__x86_64__)

struct DispatcherInfo dispatch_get()
{
    static const struct DispatcherInfo info = {
        .loop_func = dispatch_cdecl_loop,
        .quick_dispatch_func = (uintptr_t)dispatch_cdecl,
        .full_dispatch_func = (uintptr_t)dispatch_cdecl,
        .patch_data_reg = 6, // rsi
    };
    return info;
}
