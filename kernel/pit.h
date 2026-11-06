#ifndef MAXOS_PIT_H
#define MAXOS_PIT_H

#include <stdint.h>

#define PIT_HZ 100

void     pit_init(void);
uint64_t pit_ticks(void);
uint64_t uptime_ms(void);

#endif
