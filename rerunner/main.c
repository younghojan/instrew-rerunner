#include <linux/mman.h>

#include "common.h"
#include "elf-loader.h"
#include "memory.h"
#include "cpu-state.h"
#include "dispatch.h"
#include "emulate.h"

#define MAX_ARG_LENGTH 256

char dir_path[256];

int main(int argc, char **argv)
{   
    int len = strlen(argv[1]);
    if (argv[1][len - 1] != '/') {  // add '/' if argv[1] doesn't end with it
        argv[1][len] = '/';
        argv[1][len + 1] = '\0';
    }

    strncpy(dir_path, argv[1], sizeof(dir_path));
    argv[1] = strcat(argv[1], "user_args"); // path to user_args

    int fd = open(argv[1], O_RDONLY, 0);
    char cmd_line[MAX_ARG_LENGTH];
    ssize_t bytesRead = read(fd, cmd_line, sizeof(cmd_line) - 1);
    cmd_line[bytesRead] = '\0';
    close(fd);

    char *token = strtok(cmd_line, " ");
    int user_argc = atoi(token);

    int retval;

    struct State state = {0};
    signal_init(&state);

    retval = mem_init();
    if (retval < 0)
    {
        puts("error: failed to initialize heap");
        return retval;
    }

    token = strtok(NULL, " ");  // path to guest ISA binary
    BinaryInfo info = {0};
    retval = load_elf_binary(token, &info);
    if (retval != 0)
    {
        puts("error: could not load file");
        return retval;
    }

    const struct DispatcherInfo disp_info = dispatch_get();

#define STACK_SIZE 0x1000000
    int stack_prot = PROT_READ | PROT_WRITE;
    int stack_flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK;
    void *stack = mmap(NULL, STACK_SIZE, stack_prot, stack_flags, -1, 0);

    mprotect(stack, 0x1000, PROT_NONE);

    // Initialize stack according to ABI
    size_t *stack_top = (size_t *)stack + STACK_SIZE / sizeof(size_t);

    // Stack alignment
    int envc = 0;
    while (environ[envc])
        envc++;
    // user_argc = 0;
    stack_top -= (user_argc + envc) & 1; // auxv has even number of entries

    // Set auxiliary values
    *(--stack_top) = 0; // Null auxiliary vector entry

    *(--stack_top) = (uintptr_t)info.elf_entry;
    *(--stack_top) = AT_ENTRY;
    *(--stack_top) = (uintptr_t)info.phdr;
    *(--stack_top) = AT_PHDR;
    *(--stack_top) = info.phent;
    *(--stack_top) = AT_PHENT;
    *(--stack_top) = info.phnum;
    *(--stack_top) = AT_PHNUM;
    *(--stack_top) = (size_t) "x86_64";
    *(--stack_top) = AT_PLATFORM;
    *(--stack_top) = getauxval(AT_RANDOM);
    *(--stack_top) = AT_RANDOM;
    *(--stack_top) = getauxval(AT_UID);
    *(--stack_top) = AT_UID;
    *(--stack_top) = getauxval(AT_EUID);
    *(--stack_top) = AT_EUID;
    *(--stack_top) = getauxval(AT_GID);
    *(--stack_top) = AT_GID;
    *(--stack_top) = getauxval(AT_EGID);
    *(--stack_top) = AT_EGID;
    *(--stack_top) = getauxval(AT_CLKTCK);
    *(--stack_top) = AT_CLKTCK;
    *(--stack_top) = getauxval(AT_PAGESZ);
    *(--stack_top) = AT_PAGESZ;
    *(--stack_top) = 0x8001;
    *(--stack_top) = AT_HWCAP;
    *(--stack_top) = 0;
    *(--stack_top) = AT_HWCAP2;
    *(--stack_top) = 0;
    *(--stack_top) = AT_SECURE;

    *(--stack_top) = 0; // End of environment pointers

    int i;
    stack_top -= envc;
    for (i = 0; i < envc; i++)
        stack_top[i] = (uintptr_t)environ[i];
    *(--stack_top) = 0; // End of argument pointers
    stack_top -= user_argc;
    for (i = 0; i < user_argc; i++) {
        token = strtok(NULL, " ");
        stack_top[i] = (size_t)token;
    }
    *(--stack_top) = user_argc; // Argument Count

    retval = rtld_init(&state.rtld, &disp_info);

    struct CpuState *cpu_state = mem_alloc_data(sizeof(struct CpuState), _Alignof(struct CpuState));
    memset(cpu_state, 0, sizeof(*cpu_state));
    cpu_state->self = cpu_state;
    cpu_state->state = &state;

    set_thread_area(cpu_state);

    uint64_t *cpu_regs = (uint64_t *)&cpu_state->regdata;
    cpu_regs[0] = (uintptr_t)info.exec_entry;
    cpu_regs[33] = (uintptr_t)stack_top;

    disp_info.loop_func(cpu_regs);

    return 0;
}