#ifndef __KERN_PROCESS_DEBUG_H__
#define __KERN_PROCESS_DEBUG_H__
#include <proc.h>

const char* pstatestr(enum proc_state pstate);

void dump_pflags(uint32_t pflags);
void dump_pwaitstate(uint32_t waitstate);

void dump_procstruct(const struct proc_struct* ps);
void dump_procstruct_short(const struct proc_struct* ps);

enum PROC_WAITS_STRS_IDX{
    PWSTR_CHLD = 0,
    PWSTR_TIMER,
    PWSTR_KBD,
    PWSTR_KSEM,
    PWSTR_USEM,
    PWSTR_EVSEND,
    PWSTR_EVRECV,
    PWSTR_MBSEND,
    PWSTR_MBRECV,
    PWSTR_PIPE,
    PWSTR_SIGNAL,
    PWSTR_KSIGNAL,
    PWSTR_INTRED
};

#endif