#ifndef ENTRY64_H
#define ENTRY64_H
#include "../../../types.h"

void enter_usermode(void);
uint64_t enter_usermode_asm(uint64_t rip, uint64_t rsp);

#endif
